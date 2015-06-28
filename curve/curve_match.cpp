#include "curve_match.h"
#include "score.h"

#include <QDateTime>
#include <QElapsedTimer>
#include <QTextStream>
#include <QStringList>
#include <cmath>
#include <iostream>
#include <math.h>
#include <string.h>
#include <stdio.h> // for rename()

#include "functions.h"

#define PARAMS_IMPL
#include "params.h"

#include "kb_distort.h"

#define BUILD_TS (char*) (__DATE__ " " __TIME__)

#define MISC_ACCT(word, coef_name, coef_value, value) { if (abs(value) > 1E-5) { DBG("     [*] MISC %s %s %f %f", (word), (coef_name), (float) (coef_value), (float) (value)) } }

using namespace std;

/* --- main class for curve matching ---*/
CurveMatch::CurveMatch() {
  loaded = false;
  params = default_params;
  debug = false;
  done = false;
  kb_preprocess = true;
  id = -1;
}

void CurveMatch::curvePreprocess1(int curve_id) {
  /* curve preprocessing that can be evaluated incrementally :
     - evaluate turn rate
     - find sharp turns
     - normal vector calculation */

  QList<CurvePoint> oneCurve = QList<CurvePoint>();
  QList<int> idxmap = QList<int>();

  bool end_flag = false;
  for(int i = 0; i < curve.size(); i++) {
    if (curve[i].curve_id != curve_id) { continue; }
    if (curve[i].end_marker) {
      end_flag = true;
      break;
    }
    oneCurve.append(curve[i]);
    idxmap.append(i);
  }
  int l = oneCurve.size();

  for (int i = 1; i < l - 1; i ++) {
    oneCurve[i].turn_angle = (int) int(angle(oneCurve[i].x - oneCurve[i-1].x,
					     oneCurve[i].y - oneCurve[i-1].y,
					     oneCurve[i+1].x - oneCurve[i].x,
					     oneCurve[i+1].y - oneCurve[i].y) * 180 / M_PI + 0.5); // degrees


  }
  // avoid some side effects on curve_score (because there is often a delay before the second point)
  if (l > 1) {
    oneCurve[0].turn_angle = 0;
    oneCurve[l-1].turn_angle = 0;

    for (int i = 1; i < l - 1 ; i ++) {
      int t1 = oneCurve[i-1].turn_angle;
      int t2 = oneCurve[i].turn_angle;
      int t3 = oneCurve[i+1].turn_angle;

      if (abs(t2) > 160 && t1 * t2 < 0 && t2 * t3 < 0) {
	oneCurve[i].turn_angle += 360 * ((t2 < 0) - (t2 > 0));
      }
    }

    for (int i = 1; i < l - 1 ; i ++) {
      oneCurve[i].turn_smooth = int(0.5 * oneCurve[i].turn_angle + 0.25 * oneCurve[i-1].turn_angle + 0.25 * oneCurve[i+1].turn_angle);
    }
    oneCurve[0].turn_smooth = oneCurve[1].turn_angle / 4;
    oneCurve[l-1].turn_smooth = oneCurve[l-2].turn_angle / 4;
  }

  // speed evaluation
  int max_speed = 0;
  if (l > 5) {
    for(int i = 0; i < l; i ++) {
      int i1 = i - 2, i2 = i + 2;
      if (i1 < 1) { i1 = 1; i2 = 5; }
      if (i2 > l - 1) { i1 = l - 5; i2 = l - 1; }

      while(oneCurve[i1].t - oneCurve[0].t < 50 && i2 < l - 1) { i1 ++; i2 ++; }
      while(oneCurve[i2].t - oneCurve[i1].t < 40 && i2 < l - 1 && i1 > 1 ) { i2 ++; i1 --; }

      float dist = 0;
      for (int j = i1; j < i2; j ++) {
	dist += distancep(oneCurve[j], oneCurve[j+1]);
      }
      if (oneCurve[i2].t > oneCurve[i1].t) {
	oneCurve[i].speed = 1000.0 * dist / (oneCurve[i2].t - oneCurve[i1].t);
      } else {
	// compatibility with old test cases, should not happen often :-)
	if (i > 0) { oneCurve[i].speed = oneCurve[i - 1].speed; } else { oneCurve[i].speed = 1; }
      }
      if (oneCurve[i].speed > max_speed) { max_speed = oneCurve[i].speed; }
    }
  }

  for(int i = 0 ; i < l; i ++) {
    oneCurve[i].sharp_turn = 0;
  }

  if (l >= 8) { // do not process small curves, probably a simple 2-letter words @TODO handle this case in multi mode

    /* rotation / turning points */
    int sharp_turn_index = -1;
    int last_total_turn = -1;
    int last_turn_index = -100;
    int range = 1;
    for(int i = 2 ; i < l - 2; i ++) {
      float total = 0;
      float t_index = 0;
      for(int j = i - range; j <= i + range; j ++) {
	total += oneCurve[j].turn_angle;
	t_index += oneCurve[j].turn_angle * j;
      }

      if (abs(total) < last_total_turn && last_total_turn > params.turn_threshold) {
	if (sharp_turn_index >= 2 && sharp_turn_index < l - 2) {

	  for(int j = i - range; j <= i + range; j ++) {
	    if (abs(oneCurve[j].turn_angle) > params.turn_threshold2) {
	      sharp_turn_index = j;
	    }
	  }

	  int st_value = 1 + (last_total_turn > params.turn_threshold2 ||
			      abs(oneCurve[sharp_turn_index].turn_angle) > params.turn_threshold3);

	  int diff = sharp_turn_index - last_turn_index;
	  if (diff <= 1) {
	    sharp_turn_index = -1;
	  } else if (diff == 2) {
	    sharp_turn_index --;
	    oneCurve[sharp_turn_index - 1].sharp_turn = 0;
	  } else if (diff <= 4) {
	    bool remove_old = false, remove_new = false;
	    int old_value = oneCurve[last_turn_index].sharp_turn;

	    if (old_value < st_value) { remove_old = true; }
	    else if (old_value > st_value) { remove_new = true; }
	    else if (st_value == 1) {
	      remove_new = (abs(oneCurve[sharp_turn_index].turn_smooth) < abs(oneCurve[last_turn_index].turn_smooth));
	      remove_old = ! remove_new;
	    }

	    DBG("Sharp-turns too close: %d[%d] -> %d[%d] ---> remove_old=%d remove_new=%d",
		last_turn_index, old_value, sharp_turn_index, st_value, remove_old, remove_new);

	    if (remove_old) { oneCurve[last_turn_index].sharp_turn = 0; }
	    if (remove_new) { sharp_turn_index = -1; }
	  }

	  if (sharp_turn_index >= 0) {
	    oneCurve[sharp_turn_index].sharp_turn = st_value;

	    last_turn_index = sharp_turn_index;

	    DBG("Special point[%d]=%d", sharp_turn_index, oneCurve[sharp_turn_index].sharp_turn);

	    sharp_turn_index = -1;
	  }
	}
      }

      if (abs(total) > params.turn_threshold) {
	sharp_turn_index = int(0.5 + abs(t_index / total));
      }

      last_total_turn = abs(total);
    }

    // alternate way of finding turning points
    int cur = 0;
    int total, start_index, max_turn, max_index;
    int threshold = params.atp_threshold;
    int min_angle1 = params.atp_min_angle1;
    int min_turn1 = params.atp_min_turn1;
    int max_pts = params.atp_max_pts;
    int tip_gap = 0;
    int opt_gap = params.atp_opt_gap;
    int excl_gap = params.atp_excl_gap;
    bool st_found = false;
    for(int i = tip_gap; i < l - tip_gap; i ++) {
      int turn = oneCurve[i].turn_smooth; // take care of jagged curves
      if (cur) {
	st_found |= (oneCurve[i].sharp_turn != 0);
	if (abs(turn) >= threshold && cur * turn > 0 && i < l - 1 - tip_gap) {
	  // turn continues
	  total += turn;
	  if (abs(turn) > abs(max_turn)) {
	    max_turn = turn;
	    max_index = i;
	  }
	} else {
	  // turn end
	  int end_index = i;
	  if (i == l - 1 - tip_gap) { total += turn; }

	  for(int j = max(max_index - excl_gap, tip_gap); j < min(max_index + excl_gap, l - tip_gap); j ++) {
	    if (oneCurve[j].sharp_turn) { st_found = true; }
	  }
	  DBG("New turn %d->%d total=%d max_index=%d st_found=%d", start_index, end_index, total, max_index, st_found);
	  if (abs(total) > min_angle1 &&
	      abs(oneCurve[max_index].turn_smooth) >= min_turn1 &&
	      end_index - start_index <= max_pts &&
	      ! st_found) {
	    int value = 1;
	    if (start_index <= opt_gap || end_index >= l - opt_gap) { value = 5; }
	    DBG("Special point[%d]=%d (try 2)", max_index, value);
	    oneCurve[max_index].sharp_turn = value;
	  }
	  cur = 0;
	}
      } else if (abs(turn) > threshold) {
	cur = (turn > 0) - (turn < 0);
	total = turn;
	start_index = i;
	max_index = i;
	max_turn = turn;
	st_found = (oneCurve[i].sharp_turn != 0);
      }
    }

    // compute "normal" vector for turns (really lame algorithm)
    for(int i = 2; i < l - 2; i ++) {
      if (oneCurve[i].sharp_turn) {
	int sharp_turn_index = i;

	int i1 = sharp_turn_index - 1;
	int i2 = sharp_turn_index + 1;
	float x1 = oneCurve[i1].x - oneCurve[i1 - 1].x;
	float y1 = oneCurve[i1].y - oneCurve[i1 - 1].y;
	float x2 = oneCurve[i2 + 1].x - oneCurve[i2].x;
	float y2 = oneCurve[i2 + 1].y - oneCurve[i2].y;
	float l1 = sqrt(x1 * x1 + y1 * y1);
	float l2 = sqrt(x2 * x2 + y2 * y2);
	oneCurve[sharp_turn_index].normalx = 100 * (x1 / l1 - x2 / l2); // integer vs. float snafu -> don't loose too much precision
	oneCurve[sharp_turn_index].normaly = 100 * (y1 / l1 - y2 / l2);
      }
    }

    // slow down point search
    int maxd = params.max_turn_index_gap;
    for(int i = max(maxd, 0); i < l - maxd; i ++) {
      int spd0 = oneCurve[i].speed;
      if (spd0 < max_speed * params.slow_down_min) { continue; }
      int ok = 0;
      for(int j = -maxd; j <= maxd; j ++) {
	int spd = oneCurve[i + j].speed;
	if (spd < spd0 || oneCurve[i + j].sharp_turn) { ok = 0; break; }
	if (spd > params.slow_down_ratio * spd0) { ok |= (1 << (j>0)); }
      }
      if (ok == 3) {
	oneCurve[i].sharp_turn = 3;
	DBG("Special point[%d]=3", i);
      }
    }
  }

  if (end_flag) {
    oneCurve << EndMarker(curve_id);
  }

  // update aggregated curve (for logs, replay ...)
  for(int i = 0; i < idxmap.size(); i++) {
    curve[idxmap[i]] = oneCurve[i];
  }
}

void CurveMatch::curvePreprocess2() {
  /* curve preprocessing that can be deferred until final score calculation:
     - statistics
     - average speed
     - check if curve looks like a straight line (used later in scoring)
   */

  /* speed & special count */
  st.st_speed = st.st_special = 0;
  int total_speed = 0;
  for(int i = 0; i < curve.size(); i ++) {
    st.st_special += (curve[i].sharp_turn > 0);
    total_speed += curve[i].speed;
  }
  st.st_speed = total_speed / curve.size();

  /* check for straight line */
  float sc1 = 0, sc2 = 0;
  int total = 0;
  for (int i = 0; i < curve_count; i ++) {
    for (int j = 0; j < curve.size(); j ++) {
      if (curve[j].curve_id != i) { continue; }
      int turn = curve[j].turn_smooth;
      total += turn;
      sc1 = max(sc1, (float) abs(turn) / params.straight_max_turn);
    }
    sc2 = abs(total) * (0.35 + 0.65 * min(1, (float) curve.size() / 250)) / params.straight_max_total; // @todo use parameters
    float straight = max(sc1, sc2);
    logdebug("Straight curve score: %.2f (%.2f, %.2f)", straight, sc1, sc2);
    quickCurves[i].straight = straight;
  }
}

void CurveMatch::setDebug(bool debug) {
  this -> debug = debug;
}
void CurveMatch::clearKeys() {
  keys.clear();
}

void CurveMatch::addKey(Key key) {
  kb_preprocess = true;
  if (key.label >= 'a' && key.label <= 'z') {
    keys[key.label] = key;
    DBG("Key: '%c' %d,%d %d,%d", key.label, key.x, key.y, key.width, key.height);
  }
}

void CurveMatch::clearCurve() {
  curve.clear();
  done = false;
  memset(&st, 0, sizeof(st));
  curve_count = 0;
  memset(curve_started, 0, sizeof(curve_started));
}

void CurveMatch::addPoint(Point point, int curve_id, int timestamp) {
  QTime now = QTime::currentTime();

  if (curve_id < 0 || curve_id > MAX_CURVES) { return; }

  if (kb_preprocess && params.thumb_correction) {
    // we must apply keyboard biases before feeding the curve
    // in case of incremental processing
    kb_preprocess = false;
    kb_distort(keys, params);
    if (debug) {
      DBG("Keys adjustments:");
      QHashIterator<unsigned char, Key> ki(keys);
      while (ki.hasNext()) {
	ki.next();
	Key key = ki.value();
	DBG("Key '%c' %d,%d -> %d,%d", key.label, key.x, key.y, key.corrected_x, key.corrected_y);
      }
    }
  }

  if (curve.isEmpty()) {
    startTime = now;
    curve_length = 0;
  }

  int ts = (timestamp >= 0)?timestamp:startTime.msecsTo(now);

  if (curve.size() > 0) {
    if (ts < curve.last().t) {
      /* This is a workaround for a bug that produced bad test cases with
	 time going back. This should be eventually removed */
      for(int i = 0; i < curve.size(); i++) {
	curve[i].t = ts;
      }
    }

    /* Current implementation is dependant on good spacing between points
       (which is a design fault).
       Last SailfishOS has introduced some "micro-lag" (unnoticeable by
       user but it leads to "spaces" in the curve (... or maybe it's just
       some placebo effect).
       This is a simple workaround for this problem. */
    int last_index = -1;
    for(int j = curve.size() - 1; j >= 0; j --) {
      if (curve[j].curve_id == curve_id) { last_index = j; break; }
    }

    if (last_index >= 0) {
      CurvePoint lastPoint = curve[last_index];
      float dist = distancep(lastPoint, point);

      int max_length = params.max_segment_length;
      if (dist > max_length) {
	Point dp = point - lastPoint;
	int n = dist / max_length;
	for(int i = 0; i < n; i ++) {
	  float coef = (i + 1.0) / (n + 1);
	  curve << CurvePoint(lastPoint + dp * coef, curve_id, lastPoint.t + coef * (ts - lastPoint.t), curve_length + coef * dist, true /* "dummy" point */);
	}
      }
      curve_length += dist;
    }

  }

  curve_started[curve_id] = true;
  curve_count = max(curve_count, curve_id + 1);

  curve << CurvePoint(point, curve_id, ts, curve_length);
}

void CurveMatch::endOneCurve(int curve_id) {
  if (curve_id < 0 || curve_id > MAX_CURVES) { return; }
  curve << EndMarker(curve_id);
  curve_started[curve_id] = false;
}



bool CurveMatch::loadTree(QString fileName) {
  /* load a .tre (word tree) file */
  if (loaded && fileName == this -> treeFile) { return true; }
  userDictionary.clear();

  bool status = wordtree.loadFromFile(fileName);
  loaded = status;
  this -> treeFile = fileName;
  if (fileName.isEmpty()) {
    logdebug("loadtree(-): %d", status);
    this -> userDictFile = QString();
    loaded = false;

  } else if (loaded) {
    QString uf = fileName;
    if (uf.endsWith(".tre")) { uf.remove(uf.length() - 4, 4); }
    uf += "-user.txt";
    this -> userDictFile = uf;
    loadUserDict();
    logdebug("loadTree(%s): %d", QSTRING2PCHAR(fileName), status);
  }
  return status;
}

void CurveMatch::log(QString txt) {
  if (! logFile.isNull()) {
    QFile file(logFile);
    if (file.open(QIODevice::Append)) {
      QTextStream out(&file);
      out << txt << "\n";
    }
    file.close();
  }
}

void CurveMatch::setLogFile(QString fileName) {
  logFile = fileName;
}

QList<ScenarioType> CurveMatch::getCandidates() {
  return candidates;
}

QList<ScenarioDto> CurveMatch::getCandidatesDto() {
  QList<ScenarioDto> result;
  foreach(ScenarioType s, candidates) {
    ScenarioDto dto(s.getName(), s.getWordList(), s.getScore(), s.getClass(), s.getStar());
    result.append(dto);
  }
  return result;
}

void CurveMatch::scenarioFilter(QList<ScenarioType> &scenarios, float score_ratio, int min_size, int max_size, bool finished) {
  /* "skim" any scenario list based on number and/or score

     This method is awfully inefficient, but as the target is to use only the incremental
     implementation (class IncrementalMatch), it'll stay as is */

  float max_score = 0;

  qSort(scenarios.begin(), scenarios.end());  // <- 25% of the whole CPU usage, ouch!
  // (in incremental mode it's only used to sort candidates, so it's not a problem)

  foreach(ScenarioType s, scenarios) {
    float sc = s.getScore();
    if (sc > max_score) { max_score = sc; }
  }

  QHash<QString, int> dejavu;

  int i = 0;
  while (i < scenarios.size()) {
    float sc = scenarios[i].getScore();

    if (sc < max_score * score_ratio && scenarios.size() > min_size) {
      // remove scenarios with lowest scores
      st.st_skim ++;
      DBG("filtering(score): \"%s\" %s (%.3f/%.3f)", QSTRING2PCHAR(scenarios[i].getName()), QSTRING2PCHAR(scenarios[i].getId()), sc, max_score);
      scenarios.takeAt(i);

    } else if (finished || ! scenarios[i].forkLast()) {
      // remove scenarios with duplicate words (but not just after a scenario fork)

      QString name = scenarios[i].getName();
      if (dejavu.contains(name)) {
	int i0 = dejavu[name];
	scenarios.takeAt(i0);
	foreach(QString n, dejavu.keys()) {
	  if (dejavu[n] > i0) {
	    dejavu[n] --;
	  }
	}
	i --;
	dejavu.remove(name); // will be added again on next iteration
      } else {
	dejavu.insert(name, i);
	i++;
      }

    } else {
      i++;

    }

  }

  // enforce max scenarios count
  while (max_size > 1 && scenarios.size() > max_size) {
    st.st_skim ++;
    ScenarioType s = scenarios.takeAt(0);
    DBG("filtering(size): \"%s\" %s (%.3f/%.3f)", QSTRING2PCHAR(s.getName()), QSTRING2PCHAR(s.getId()), s.getScore(), max_score);
  }

}

void CurveMatch::setCurves() {
  for (int i = 0; i < curve_count; i ++) {
    curvePreprocess1(i);
    quickCurves[i].setCurve(curve, i, params.multi_dot_threshold);
  }
  quickCurves[curve_count].clearCurve();
}

bool CurveMatch::match() {
  /* run the full "one-shot algorithm */
  scenarios.clear();
  candidates.clear();

  if (! loaded || ! keys.size() || ! curve.size()) { return false; }
  if (curve.size() < 3) { return false; }

  memset(& st, 0, sizeof(st));

  // change order for equal items: qSort(curve.begin(), curve.end()); // in multi mode we may lose point ordering

  QuickKeys quickKeys(keys);

  setCurves();
  curvePreprocess2();

  ScenarioType root = ScenarioType(&wordtree, &quickKeys, quickCurves, &params);
  root.setDebug(debug);
  scenarios.append(root);

  QElapsedTimer timer;
  timer.start();

  int count = 0;

  int n = 0;
  while (scenarios.size()) {
    QList<ScenarioType> new_scenarios;
    foreach(ScenarioType scenario, scenarios) {

      QList<ScenarioType> childs;
      scenario.nextKey(childs, st);
      foreach(ScenarioType child, childs) {
	if (child.isFinished()) {
	  if (child.postProcess()) {
	    DBG("New candidate: %s (score=%.3f)", QSTRING2PCHAR(child.getId()), child.getScore());
	    candidates.append(child);
	  } else {
	    DBG("Failed candidate: %s", QSTRING2PCHAR(child.getId()));
	  }
	} else {
	  new_scenarios.append(child);
	}
	count += 1;
      }

    }
    n += 1;
    scenarios = new_scenarios; // remember QT collections have intelligent copy-on-write (hope it works)

    if (debug) {
      foreach(ScenarioType scenario, scenarios) {
	DBG("LST[%d] %s [%.2f]", n, QSTRING2PCHAR(scenario.getId()), scenario.getScore());
      }
    }

    if (n >= 3) {
      // scenario filtering works differently in non-incremental mode
      // as we do not care about performance in this case, just arbitrary increase max_active_scenarios value
      scenarioFilter(scenarios, 0, 15, params.max_active_scenarios * 1.5, false); // @todo add to parameter list
      DBG("Depth: %d - Scenarios: %d - Candidates: %d", n, scenarios.size(), candidates.size());
    }
  }

  st.st_time = (int) timer.elapsed();
  st.st_count = count;

  scenarioFilter(candidates, 0.5, 10, 3 * params.max_candidates, true); // @todo add to parameter list
  sortCandidates();
  scenarioFilter(candidates, 0.7, 10, params.max_candidates, true); // @todo add to parameter list

  logdebug("Candidates: %d (time=%d, nodes=%d, forks=%d, skim=%d, speed=%d, special=%d)", candidates.size(), st.st_time, st.st_count, st.st_fork, st.st_skim, st.st_speed, st.st_special);

  done = true;

  return candidates.size() > 0;
}

void CurveMatch::sortCandidates() {
  QList <ScenarioType *> pcandidates = QList<ScenarioType *>();
  QListIterator<ScenarioType> it(candidates);
  while(it.hasNext()) {
    pcandidates.append((ScenarioType*) &(it.next()));
  }

  ScenarioType::sortCandidates(pcandidates, params, debug);
}

void CurveMatch::endCurve(int correlation_id) {
  for (int i = 0; i < MAX_CURVES; i ++) {
    if (curve_started[i]) {
      curve << EndMarker(i);
      DBG("Implicit curve end #%d", i);
    }
  }
  this -> id = correlation_id;
  log(QString("IN: ") + toString());
  if (! done) { match(); }
  log(QString("OUT: ") + resultToString());
}

void CurveMatch::setParameters(QString jsonStr) {
  QJsonDocument doc = QJsonDocument::fromJson(jsonStr.toUtf8());
  params = Params::fromJson(doc.object());
}

void CurveMatch::useDefaultParameters() {
  params = default_params;
}

QList<CurvePoint> CurveMatch::getCurve() {
  return curve;
}

void CurveMatch::toJson(QJsonObject &json) {
  json["id"] = id;

  // parameters
  QJsonObject json_params;
  params.toJson(json_params);
  json["params"] = json_params;

  // keys
  QJsonArray json_keys;
  QHash<unsigned char, Key>::iterator i;
  for (i = keys.begin(); i != keys.end(); ++i) {
    Key k = i.value();
    QJsonObject json_key;
    k.toJson(json_key);
    json_keys.append(json_key);
  }
  json["keys"] = json_keys;

  // curve
  QJsonArray json_curve;
  foreach(CurvePoint p, curve) {
    QJsonObject json_point;
    p.toJson(json_point);
    json_curve.append(json_point);
  }
  json["curve"] = json_curve;

  // other
  json["treefile"] = treeFile;
  json["datetime"] = QDateTime::currentDateTime().toString(Qt::ISODate);
}

void CurveMatch::fromJson(const QJsonObject &json) {

  QJsonValue input = json["input"];
  if (input != QJsonValue::Undefined) {
    fromJson(input.toObject());
    return;
  }

  // parameters
  params = Params::fromJson(json["params"].toObject());

  // keys
  QJsonArray json_keys = json["keys"].toArray();
  keys.clear();
  foreach(QJsonValue json_key, json_keys) {
    Key k = Key::fromJson(json_key.toObject());
    keys[k.label] = k;
  }

  // curve
  QJsonArray json_curve = json["curve"].toArray();
  curve.clear();
  foreach(QJsonValue json_point, json_curve) {
    QJsonObject json_object = json_point.toObject();
    CurvePoint p = CurvePoint::fromJson(json_object);
    if (! p.dummy) {
      curve.append(p);
    }
  }
}

void CurveMatch::fromString(const QString &jsonStr) {
  QJsonDocument doc = QJsonDocument::fromJson(jsonStr.toUtf8());
  fromJson(doc.object());
}

QString CurveMatch::toString(bool indent) {
  QJsonObject json;
  toJson(json);
  QJsonDocument doc(json);
  return QString(doc.toJson(indent?QJsonDocument::Indented:QJsonDocument::Compact));
}

void CurveMatch::resultToJson(QJsonObject &json) {
  json["id"] = id;
  json["build"] = QString(BUILD_TS);

  QJsonObject json_input;
  toJson(json_input);
  json["input"] = json_input;

  QJsonArray json_candidates;
  QList<ScenarioType> candidates = getCandidates();
  qSort(candidates.begin(), candidates.end());
  foreach (ScenarioType c, candidates) {
    QJsonObject json_scenario;
    c.toJson(json_scenario);
    json_candidates.append(json_scenario);
  }
  json["candidates"] = json_candidates;
  json["ts"] = QDateTime::currentDateTime().toString(Qt::ISODate);

  QJsonObject json_stats;
  json_stats["time"] = st.st_time;
  json_stats["count"] = st.st_count;
  json_stats["fork"] = st.st_fork;
  json_stats["skim"] = st.st_skim;
  json_stats["special"] = st.st_special;
  json_stats["speed"] = st.st_speed;
  json_stats["cache_hit"] = st.st_cache_hit;
  json_stats["cache_miss"] = st.st_cache_miss;
  json["stats"] = json_stats;

  QJsonObject json_params;
  default_params.toJson(json_params);
  json["default_params"] = json_params;

  QJsonArray json_st;
  for(int i = 0; i < curve_count; i ++) {
    json_st.append(quickCurves[i].straight);
  }
  json["straight"] = json_st;
}

QString CurveMatch::resultToString(bool indent) {
  QJsonObject json;
  resultToJson(json);
  QJsonDocument doc(json);
  return QString(doc.toJson(indent?QJsonDocument::Indented:QJsonDocument::Compact));
}

void CurveMatch::learn(QString letters, QString word, bool init) {
  DBG("CM-learn [%s]: %s (%d)", QSTRING2PCHAR(letters), QSTRING2PCHAR(word), (int) init);

  if (letters.length() < 2 || word.length() < 2) { return; }
  if (! params.user_dict_learn) { return; }
  if (! loaded) { return; }

  QString payload_word = word;
  if (word == letters) {
    payload_word = QString("=");
  }

  // get user dictionary
  UserDictEntry entry;
  if (userDictionary.contains(word)) {
    entry = userDictionary[word];
  }

  // get data from in-memory tree
  QPair<void*, int> pl = wordtree.getPayload(QSTRING2PUCHAR(letters));

  // compute new node value for in-memory tree
  QString payload;
  if (pl.first) { // existing node
    payload = QString((const char*) pl.first);
    QStringList lst = payload.split(",");
    bool found = false;
    foreach(QString w, lst) {
      if (w == payload_word) {
	found = true; // we already know this word
      }
    }
    if (found) {
      payload = QString(); // don't update the tree
    } else {
      lst.append(payload_word);
      payload = lst.join(",");
    }
  } else { // new node
    payload = payload_word;
  }

  // update user dictionary (and in-memory tree)
  int now = (int) time(0);
  if (! payload.isEmpty()) {
    if (! init) { logdebug("Learned new word: %s [%s]", QSTRING2PCHAR(word), QSTRING2PCHAR(letters)); }
    unsigned char *ptr = QSTRING2PUCHAR(payload);
    int len = strlen((char*) ptr) + 1;
    unsigned char pl_char[len];
    memmove(pl_char, ptr, len);
    wordtree.setPayload(QSTRING2PUCHAR(letters), pl_char, len);
    DBG("CM-learn: update tree %s -> '%s' pl=[%s]", QSTRING2PCHAR(letters), QSTRING2PCHAR(word), pl_char);
  }

  if (! entry.letters.isEmpty() || ! payload.isEmpty()) {
    // if we've added the word to our memory tree or the word already exist in user dictionary
    userdict_dirty = true;
    float new_count = entry.count + (init?0.0:1.0);
    userDictionary[word] = UserDictEntry(letters, now, new_count);
    logdebug("CM-learn: update user dict '%s' new_count=%.2f", QSTRING2PCHAR(word), new_count);
  }

}

void CurveMatch::loadUserDict() {
  if (userDictFile.isEmpty()) { return; }

  userdict_dirty = false;
  userDictionary.clear();

  QFile file(userDictFile);
  if (! file.open(QIODevice::ReadOnly)) { return; }

  QTextStream in(&file);

  QString line;
  do {
    line = in.readLine();
    QTextStream stream(&line);
    QString word, letters;
    float count;
    int ts;

    stream >> word >> letters >> count >> ts;

    if (count && ts) {
      userDictionary[word] = UserDictEntry(letters, ts, count);
      learn(letters, word, true); // add the word to in memory tree dictionary
    }
  } while (!line.isNull());

  file.close();
  purgeUserDict();
}

void CurveMatch::saveUserDict() {
  if (! userdict_dirty) { return; }
  if (userDictFile.isEmpty()) { return; }
  if (userDictionary.isEmpty()) { return; }

  purgeUserDict();

  QFile file(userDictFile + ".tmp");
  if (! file.open(QIODevice::WriteOnly)) { return; }

  QTextStream out(&file);

  int now = (int) time(0);

  QHashIterator<QString, UserDictEntry> i(userDictionary);
  while (i.hasNext()) {
    i.next();
    UserDictEntry entry = i.value();
    QString word = i.key();
    if (! entry.expire(now) && ! word.isEmpty() && ! entry.letters.isEmpty()) {
      out << word << " " << entry.letters << " " << entry.count << " " << entry.ts << endl;
    }
  }

  file.close();
  if (file.error()) { return; /* save failed */ }

  // QT rename can't do an atomic file replacement
  rename(QSTRING2PCHAR(userDictFile + ".tmp"), QSTRING2PCHAR(userDictFile));

  userdict_dirty = false;
}

float UserDictEntry::score() const {
  return -ts; // we'll just do a simple LRU eviction for now
}

bool UserDictEntry::expire(int /* current time not used yet */) {
  return false;
}

static int userDictLessThan(QPair<QString, float> &e1, QPair<QString, float> &e2) {
  return e1.second < e2.second;
}


void CurveMatch::purgeUserDict() {
  if (userDictionary.size() <= params.user_dict_size + 100) { return; }

  userdict_dirty = true;

  QList<QPair<QString, float> > lst;
  QHashIterator<QString, UserDictEntry> i(userDictionary);
  while (i.hasNext()) {
    i.next();
    float score = i.value().score();
    lst.append(QPair<QString, float>(i.key(), score));
  }

  qSort(lst.begin(), lst.end(), userDictLessThan);

  for(int i = 0; i < lst.size() - params.user_dict_size; i ++) {
    userDictionary.remove(lst[i].first);
  }
}

void CurveMatch::dumpDict() {
  wordtree.dump();
}

char* CurveMatch::getPayload(unsigned char *letters) {
  return (char*) wordtree.getPayload(letters).first;
}
