#include <QTextStream>

#include <math.h>

#include "multi.h"
#include "params.h"

#define FOREACH_ALL_SCENARIOS(var, code) for(QList<QSharedPointer<Scenario> >::const_iterator it = scenarios.begin(); it != scenarios.end(); ++ it) { Scenario *var = it->data(); code; }
#define S(curve_id) (scenarios[curve_id].data())

MultiScenario::MultiScenario(LetterTree *tree, QuickKeys *keys, QuickCurve **curves, Params *params) {
  this -> node = tree -> getRoot();
  this -> keys = keys;
  this -> curves = curves;
  this -> params = params;

  debug = dead = finished = false;
  count = 0;
  dist = dist_sqr = 0;
  curve_count = 0;

  temp_score = 0;
  final_score = final_score2 = final_score_v1 = -1;

  ts = 0;

  history = new history_t[1];
  letter_history = new unsigned char[1];
  letter_history[0] = '\0';
}

MultiScenario::MultiScenario(const MultiScenario &from) {
  copy_from(from);
}

MultiScenario& MultiScenario::operator=(const MultiScenario &from) {
  // overriden copy to take care of dynamically allocated stuff
  delete[] history;
  delete[] letter_history;
  copy_from(from);
  return *this;
}

void MultiScenario::copy_from(const MultiScenario &from) {
  params = from.params;
  curves = from.curves;
  params = from.params;
  keys = from.keys;

  node = from.node;
  finished = from.finished;
  count = from.count;
  curve_count = from.curve_count;

  debug = from.debug;
  dead = from.dead; // probably useless :-)

  ts = from.ts;

  temp_score = from.temp_score;
  final_score = from.final_score;
  final_score2 = from.final_score2;
  final_score_v1 = from.final_score_v1;

  history = new history_t[count + 1];
  letter_history = new unsigned char[count + 2]; // one more char to allow to keep a '\0' at the end (for use for display as a char*)

  memcpy(letter_history, from.letter_history, count * sizeof(unsigned char));
  memcpy(history, from.letter_history, count * sizeof(history_t));

  letter_history[count] = '\0';

  scenarios = from.scenarios; // just copy smart pointers
}

MultiScenario::~MultiScenario() {
  delete[] history;
  delete[] letter_history;
}

QString MultiScenario::getId() const {
  QString ret;
  QTextStream ts(& ret);
  int last_curve = -1;
  for(int i = 0; i < count; i++) {
    int curve = history[i].curve_id;
    if (curve != last_curve) {
      if (last_curve != -1) { ts << "] "; }
      ts << "[" << curve << ":";
      last_curve = curve;
    }
    ts << letter_history[i];
    if (history[i].end_curve) { ts << "*"; }
  }
  ts << "]";

  return ret;
}

bool MultiScenario::childScenario(LetterNode &childNode, bool endScenario, QList<MultiScenario> &result, int &st_fork, bool incremental) {
  unsigned char letter = childNode.getChar();

  if (curve_count > scenarios.size()) {
    Scenario *s = new Scenario((LetterTree*) NULL /* no more needed? */, keys, curves[curve_count - 1], params);
    s->setDebug(debug);
    scenarios[curve_count - 1] = QSharedPointer<Scenario>(s);
  }

  int in_progress = 0;
  for (int curve_id = 0; curve_id < curve_count; curve_id ++) {
    in_progress += (! S(curve_id)->isFinished());
  }
  if (endScenario && in_progress >= 2) { return true; } // last iteration and 2 curves still in progress

  if (curve_count >= 2) { DBG("[MULTI] current scenario: %s + '%c' [end=%d]", QSTRING2PCHAR(getId()), letter, endScenario); }

  for (int curve_id = 0; curve_id < curve_count; curve_id ++) {
    QuickCurve curve = (*curves)[curve_id];

    Scenario *scenario = S(curve_id);
    if (scenario->isFinished()) { continue; } // we have entirely matched this curve

    for(int end = 0; end <= 1; end ++) {
      if (endScenario && ! end) { continue; }
      if (curve_count <= 1 && end && ! endScenario) { continue; }
      if (end && ! curve.finished) { continue; } // we assume we were not called before min length (in incremental mode)

      if (curve_count >= 2) { DBG("[MULTI] curve_id=%d end=%d", curve_id, end); }

      // @todo heuristics to be added here (or we may explore too much)
      // @todo evaluate most likely scenario first, and ignore other if it succeed
      // @todo evaluate metrics for incremental mode (min / max length for next checks
      // @todo handle one-key curves (simple clicks)

      QList<Scenario> childs;
      if (! scenario -> childScenario(childNode, end == 1, childs, st_fork, incremental)) { return false; }

      foreach(Scenario child, childs) {
	int new_ts = child.getTimestamp();
	if (new_ts < ts - params->multi_max_time_rewind) { continue; } // we accept small infraction with ordering :-)

	MultiScenario new_ms(*this);
	new_ms.scenarios[curve_id] = QSharedPointer<Scenario>(new Scenario(child)); // useless copy -> @todo Scenario::childScenario return pointers to dynamically allocated objects
	new_ms.history[count].curve_id = curve_id;
	new_ms.history[count].curve_index = child.getCurveIndex();
	new_ms.history[count].index = child.getCount() - 1;
	new_ms.history[count].end_curve = (end == 1);
	new_ms.letter_history[count] = letter;
	new_ms.count = count + 1;
	new_ms.finished = endScenario;
	new_ms.dist_sqr = child.getDistSqr() - scenario->getDistSqr();
	new_ms.dist = sqrt(new_ms.dist_sqr);
	new_ms.ts = new_ts;

	if (curve_count >= 2) { DBG("[MULTI] -> child scenario: %s [end=%d]", QSTRING2PCHAR(new_ms.getId()), endScenario); }

	result.append(new_ms);
      }

    }

  }
  return true;
}


void MultiScenario::nextKey(QList<MultiScenario> &result, int &st_fork) {
  foreach (LetterNode child, node.getChilds()) {
    if (child.hasPayload() && count >= 1) {
      childScenario(child, true, result, st_fork);
    }
    childScenario(child, false, result, st_fork);
  }
}

bool MultiScenario::operator<(const MultiScenario &other) const {
  if (this -> getScore() == -1 || other.getScore() == -1) {
    return this -> distance() / sqrt(this -> getCount()) > other.distance() / sqrt(other.getCount());
  } else {
    return this -> getScore() < other.getScore();
  }
}

QString MultiScenario::getName() {
  QString ret;
  ret.append((char*) getNameCharPtr());
  return ret;
}

unsigned char* MultiScenario::getNameCharPtr() const {
  return letter_history;
}

QString MultiScenario::getWordList() {
  QPair<void*, int> payload = node.getPayload();
  return QString((char*) payload.first); // payload is always zero-terminated
}

float MultiScenario::getTempScore() const {
  return count?temp_score / count:0;
}

float MultiScenario::getScore() const {
  if (final_score2 != -1) { return final_score2; }
  return (final_score == -1)?getTempScore():final_score;
}

float MultiScenario::getCount() const {
  return count;
}

bool MultiScenario::forkLast() {
  FOREACH_ALL_SCENARIOS(s, {
      if (s->forkLast()) { return true; }
    });
  return false;
}

static void score_t_add(score_t &to, score_t &from, float coef) {
  to.distance_score += coef * from.distance_score;
  to.turn_score     += coef * from.turn_score;
  to.cos_score      += coef * from.cos_score;
  to.curve_score    += coef * from.curve_score;
  to.misc_score     += coef * from.misc_score;
  to.length_score   += coef * from.length_score;
}

static void score_t_min(score_t &to, score_t &from) {
  to.distance_score = min(to.distance_score, from.distance_score);
  to.turn_score     = min(to.turn_score,     from.turn_score);
  to.cos_score      = min(to.cos_score,      from.cos_score);
  to.curve_score    = min(to.curve_score,    from.curve_score);
  to.misc_score     = min(to.misc_score,     from.misc_score);
  to.length_score   = min(to.length_score,   from.length_score);
}

static void score_t_clear(score_t &s) {
  s.distance_score = s.turn_score = s.cos_score = s.curve_score = s.misc_score = s.length_score = 0; 
}

void MultiScenario::evalScore(float &score, float &min_score, score_t &avg, score_t &min) const {
  float total = 0;

  float avg_coef = 1.0 / count;
  bool first = true;

  min_score = 0;
  score_t_clear(avg);
  score_t_clear(min);

  FOREACH_ALL_SCENARIOS(s, {
      score_t mss = s->getMinScores();
      score_t avs = s->getScores();
      if (first) {
	memcpy(&min, &mss, sizeof(score_t));
	first = false;
      } else {
	score_t_min(min, mss);
      }
      score_t_add(avg, avs, avg_coef * s->getCount());
      total += s->getScore();
      float ms = s->getMinScore();
      min_score = (ms > min_score || ! min_score)?ms:min_score;
    });
  score = total / count;
}

void MultiScenario::getDetailedScores(score_t &avg, score_t &min) const {
  float score, min_score;
  evalScore(score, min_score, avg, min);
}

score_t MultiScenario::getScores() {
  score_t avg, min;
  float score, min_score;
  evalScore(score, min_score, avg, min);
  return avg;
}

score_t MultiScenario::getMinScores() {
  score_t avg, min;
  float score, min_score;
  evalScore(score, min_score, avg, min);
  return min;
}

 float MultiScenario::getMinScore() {
  score_t avg, min;
  float score, min_score;
  evalScore(score, min_score, avg, min);
  return min_score;
 }

int MultiScenario::getErrorCount() {
  int ret = 0;
  FOREACH_ALL_SCENARIOS(s, { ret += s->getErrorCount(); } );
  return ret;
}

int MultiScenario::getGoodCount() {
  int ret = 0;
  FOREACH_ALL_SCENARIOS(s, { ret += s->getGoodCount(); } );
  return ret;
}

float MultiScenario::distance() const {
  return dist;
}

static void scoreToJson(QJsonObject &json_obj, score_t &score) {
  json_obj["score_distance"] = score.distance_score;
  json_obj["score_cos"] = score.cos_score;
  json_obj["score_turn"] = score.turn_score;
  json_obj["score_curve"] = score.curve_score;
  json_obj["score_misc"] = score.misc_score;
  json_obj["score_length"] = score.length_score;
}

void MultiScenario::toJson(QJsonObject &json) {
  score_t avg, min;
  float score, min_score;
  evalScore(score, min_score, avg, min);

  json["name"] = getName();
  json["finished"] = finished;
  json["score"] = getScore();
  json["score_std"] = getScoreOrig();
  json["score_v1"] = final_score_v1;
  json["distance"] = distance();
  json["class"] = 0;
  json["star"] = 0;
  json["min_total"] = min_score;
  json["error"] = getErrorCount();
  json["good"] = getGoodCount();
  json["words"] = getWordList();

  QJsonArray json_score_array;
  for(int i = 0; i < count; i ++) {
    QJsonObject json_score;
    json_score["letter"] = QString(letter_history[i]);
    json_score["curve_id"] = history[i].curve_id;
    json_score["index"] = history[i].curve_index;
    score_t sc = S(history[i].curve_id)->getScoreIndex(history[i].index);
    scoreToJson(json_score, sc);
    json_score_array.append(json_score);
  }
  json["detail"] = json_score_array;

  QJsonObject json_min, json_avg;
  scoreToJson(json_min, min);
  scoreToJson(json_avg, avg);
  json["avg_score"] = json_avg;
  json["min_score"] = json_min;
}

QString MultiScenario::toString(bool indent) {
  QJsonObject json;
  toJson(json);
  QJsonDocument doc(json);
  return QString(doc.toJson(indent?QJsonDocument::Indented:QJsonDocument::Compact));
}

