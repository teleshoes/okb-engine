#include "multi.h"

#ifdef MULTI

#include <QTextStream>
#include <math.h>

#include "params.h"

#define FOREACH_ALL_SCENARIOS(var, code) for(QList<QSharedPointer<Scenario> >::const_iterator it = scenarios.begin(); it != scenarios.end(); ++ it) { Scenario *var = it->data(); code; }
#define S(curve_id) (scenarios[curve_id].data())

int MultiScenario::global_id = -1;
QList<Scenario> MultiScenario::scenario_root;

MultiScenario::MultiScenario(LetterTree *tree, QuickKeys *keys, QuickCurve *curves, Params *params) {
  this -> node = tree -> getRoot();
  this -> keys = keys;
  this -> curves = curves;
  this -> params = params;

  debug = finished = false;
  count = 0;
  dist = dist_sqr = 0;
  curve_count = 0;

  final_score = -1;

  ts = 0;

  history = new history_t[1];
  letter_history = new unsigned char[1];
  letter_history[0] = '\0';

  zombie = false;

  id = 0;
  MultiScenario::global_id = 1;
  MultiScenario::scenario_root.clear();
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

  dist = from.dist;
  dist_sqr = from.dist_sqr;

  ts = from.ts;

  final_score = from.final_score;

  history = new history_t[count + 1];
  letter_history = new unsigned char[count + 2]; // one more char to allow to keep a '\0' at the end (for use for display as a char*)

  if (count) {
    memcpy(letter_history, from.letter_history, count * sizeof(unsigned char));
    memcpy(history, from.history, count * sizeof(history_t));
  }

  letter_history[count] = letter_history[count + 1] = '\0';

  scenarios = from.scenarios; // just copy smart pointers

  id = from.id;
  zombie = from.zombie;
}

MultiScenario::~MultiScenario() {
  delete[] history;
  delete[] letter_history;
}

QString MultiScenario::getId() const {
  QString ret;
  QTextStream ts(& ret);

  ts << "@" << id << " ";

  int last_curve = -1;
  for(int i = 0; i < count; i++) {
    int curve = history[i].curve_id;
    if (curve != last_curve) {
      if (last_curve != -1) { ts << "] "; }
      ts << "[" << curve << ":";
      last_curve = curve;
    }
    ts << (char) letter_history[i];
    if (history[i].end_curve) { ts << "*"; }
  }
  if (! count) { ts << "[root"; }
  ts << "]";
  if (zombie) { ts << " (Z)"; }
  ts << " \"";
  ts << (char*) letter_history << "\"";

  return ret;
}

void MultiScenario::addSubScenarios() {
  while (curve_count > scenarios.size()) {
    Scenario *s;

    // reuse scenario root for cache sharing
    int idx = scenarios.size();
    if (idx < MultiScenario::scenario_root.size()) {
      s = new Scenario(MultiScenario::scenario_root[idx]);
    } else {
      s = new Scenario((LetterTree*) NULL /* no more needed? */, keys, &(curves[scenarios.size()]), params);
      s->setDebug(debug);
      s->setCache(true); // improve performance for scenario reuse
      MultiScenario::scenario_root.append(*s);
    }

    scenarios.append(QSharedPointer<Scenario>(s));
  }
}

bool MultiScenario::childScenario(LetterNode &childNode, QList<MultiScenario> &result, stats_t &st, int filter_curve_id, bool incremental) {
  unsigned char letter = childNode.getChar();

  while (curve_count < MAX_CURVES && (curves[curve_count].getCount() > 0)) {
    // find how many curve we've got (ugly)
    DBG("[MULTI] new curve added (index=#%d)", curve_count);
    curve_count ++;
  }

  addSubScenarios();

  if (zombie) {
    // zombie scenario: only check for double letters at the end of sub-scenarios
    bool found = false;
    for(int curve_id = 0; curve_id < curve_count; curve_id ++) {
      Scenario *scenario = S(curve_id);
      int scenario_count = scenario->getCount();
      if (scenario_count > 0) {
	unsigned char last_letter = scenario->getNameCharPtr()[scenario_count - 1];
	if (last_letter == letter) { found = true; break; }
      }
    }
    // DBG("Zombie scenario: %s (letter='%c', found=%d, filter_curve_id=%d)", QSTRING2PCHAR(getId()), letter, found, filter_curve_id);
    if (! found) {
      return true;
    }
  }


  bool hasPayload = childNode.hasPayload();
  bool isLeaf = childNode.isLeaf();

  int in_progress = 0;
  for (int curve_id = 0; curve_id < curve_count; curve_id ++) {
    in_progress += (! S(curve_id)->isFinished());
  }
  if (! in_progress && ! zombie) { return true; }
  if (isLeaf && in_progress >= 2) { return true; } // last iteration and 2 curves still in progress

  if (curve_count >= 2) { DBG("[MULTI] current scenario: %s + '%c' -> \"%s:%c\"", QSTRING2PCHAR(getId()), letter, getNameCharPtr(), letter); }

  for (int curve_id = 0; curve_id < curve_count; curve_id ++) {
    if (filter_curve_id >=0 && curve_id != filter_curve_id) { continue; }

    Scenario *scenario = S(curve_id);
    if (scenario->isFinished() && ! zombie) { continue; } // we have entirely matched this curve

    QuickCurve *curve = &(curves[curve_id]);

    bool chld_hasPayload = (in_progress > 1)?true:hasPayload;
    bool chld_isLeaf = isLeaf;
    /* if (curve_count >= 2) */ { DBG("[MULTI] %s + '%c' curve_id=%d [hasPayload=%d->%d isLeaf=%d] in_progress=%d curve_finished=%d count=%d",
				      QSTRING2PCHAR(getId()), letter, curve_id, hasPayload, chld_hasPayload, chld_isLeaf, in_progress, curve->finished, count); }

    bool zombie_if_finished = false;
    if (curve_count > 1 && in_progress == 1 && ! chld_hasPayload) {
      // we are closing the last curve, but there is no exact match
      // keep the scenario alive in case there are some additionnal double letters at the end
      zombie_if_finished = true;
      chld_hasPayload = true;
    }

    QList<Scenario> childs;

    int scenario_count = scenario->getCount();
    if (scenario_count > 0 && letter == scenario->getNameCharPtr()[scenario_count - 1]) {
      DBG("%s: Dual letter '%c'", QSTRING2PCHAR(getId()), letter);
      // handle multi-scenarios that lead to dual-letter single scenarios
      // @todo move this into Scenario::childScenario to handle dual-letter in a more general way and support dual-letter user hints
      childs.append(*scenario);
      zombie = false;
      zombie_if_finished = false;

    } else if (zombie) {
      // zombie scénario has not matched any letter with above tets --> give up
      continue; // try other curves ...
    } else if (! scenario -> childScenario(childNode, childs, st, 0, incremental, chld_hasPayload, chld_isLeaf)) { return false; }

    // @todo optional zone notion. e.g. left & right half keyboard. A curve is contained in a single region (should reduce tree width)

    foreach(Scenario child, childs) {
      int new_ts = child.getTimestamp();
      if (new_ts < ts - params->multi_max_time_rewind) { continue; } // we accept small infraction with events ordering :-)

      bool endScenario = child.isFinished();

      MultiScenario new_ms(*this);
      new_ms.scenarios[curve_id] = QSharedPointer<Scenario>(new Scenario(child)); // useless copy -> @todo Scenario::childScenario return pointers to dynamically allocated objects
      new_ms.node = childNode;
      new_ms.history[count].curve_id = curve_id;
      new_ms.history[count].curve_index = child.getCurveIndex();
      new_ms.history[count].index = child.getCount() - 1;
      new_ms.history[count].end_curve = endScenario;
      new_ms.letter_history[count] = letter;
      new_ms.count = count + 1;
      new_ms.finished = endScenario && (in_progress <= 1) && (! zombie_if_finished); // we've just matched the last point in the last curve
      new_ms.dist_sqr = dist_sqr + child.getDistSqr() - scenario->getDistSqr();
      new_ms.dist = sqrt(new_ms.dist_sqr / (count + 1));
      new_ms.ts = new_ts;
      new_ms.id = (MultiScenario::global_id ++);
      if (endScenario && zombie_if_finished) { new_ms.zombie = true; } // @todo check again. it probably does not cover all cases.

      /* if (curve_count >= 2) */ { DBG("[MULTI] -> child scenario: %s [end=%d, zombie=%d]", QSTRING2PCHAR(new_ms.getId()), endScenario, new_ms.zombie); }

      result.append(new_ms);
    }

  }

  /* if (curve_count >= 2) */ { DBG("[MULTI] iteration OK"); DBG(" "); }
  return true;
}


void MultiScenario::nextKey(QList<MultiScenario> &result, stats_t &st) {
  foreach (LetterNode child, node.getChilds()) {
    childScenario(child, result, st);
    // note: childScernario return value is ignored because this method is not
    // used with incremental mode
  }
}

bool MultiScenario::operator<(const MultiScenario &other) const {
  return this -> getScore() < other.getScore();
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

float MultiScenario::getScore() const {
  if (! count) { return 0; }
  float score = 0;
  FOREACH_ALL_SCENARIOS(s, {
      score += (finished?s->getScore():s->getTempScore()) * s->getCount();
    });
  return score / count;
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

bool MultiScenario::postProcess() {
  bool result = true;
  FOREACH_ALL_SCENARIOS(s, {
      result &= s->postProcess();
    });

  this->final_score = getScore();

  return result;
}

static void score_t_add(score_t &to, score_t &from, float coef) {
  to.distance_score += coef * from.distance_score;
  to.turn_score     += coef * from.turn_score;
  to.cos_score      += coef * from.cos_score;
  to.curve_score    += coef * from.curve_score;
  to.misc_score     += coef * from.misc_score;
  to.length_score   += coef * from.length_score;
}

static void score_t_clear(score_t &s) {
  s.distance_score = s.turn_score = s.cos_score = s.curve_score = s.misc_score = s.length_score = 0;
}

score_t MultiScenario::getScores() {
  score_t ret;
  score_t_clear(ret);

  FOREACH_ALL_SCENARIOS(s, {
      score_t sc = s->getScores();
      score_t_add(ret, sc, 1.0 * s->getCount() / count);
    } );

  return ret;
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
  json["name"] = getName();
  json["finished"] = finished;
  json["score"] = getScore();
  json["score_v1"] = -1;
  json["distance"] = (int) distance();
  json["class"] = 0;
  json["star"] = 0;
  json["error"] = getErrorCount();
  json["good"] = getGoodCount();
  json["words"] = getWordList();
  json["id"] = getId();

  QJsonArray json_score_array;
  for(int i = 0; i < count; i ++) {
    QJsonObject json_score;
    json_score["letter"] = QString(letter_history[i]);
    json_score["curve_id"] = history[i].curve_id;
    json_score["index"] = history[i].index;
    json_score["curve_index"] = history[i].curve_index;
    score_t sc = S(history[i].curve_id)->getScoreIndex(history[i].index);
    scoreToJson(json_score, sc);
    json_score_array.append(json_score);
  }
  json["detail"] = json_score_array;

  QJsonObject json_avg;
  score_t avg_score = getScores();
  scoreToJson(json_avg, avg_score);
  json["avg_score"] = json_avg;

  QJsonArray json_scenarios;
  FOREACH_ALL_SCENARIOS(s, {
      QJsonObject json_scenario;
      s->toJson(json_scenario);
      json_scenarios.append(json_scenario);
    });
  json["scenarios"] = json_scenarios;

  json["multi"] = 1;
}

QString MultiScenario::toString(bool indent) {
  QJsonObject json;
  toJson(json);
  QJsonDocument doc(json);
  return QString(doc.toJson(indent?QJsonDocument::Indented:QJsonDocument::Compact));
}

void MultiScenario::sortCandidates(QList<MultiScenario *> candidates, Params &params, int debug) {
  if (candidates.size() == 0) { return; }
  int curve_count = candidates[0]->curve_count;

  QList<QList<Scenario *> > scenarios;
  for (int i = 0; i < curve_count; i ++) {
    scenarios.append(QList<Scenario *>());
  }
  for(int i = candidates.size() - 1; i >=0; i --) {
    for(int j = 0; j < curve_count; j ++) {
      scenarios[j].append(candidates[i]->scenarios[j].data());
    }
  }

  for(int i = 0; i < curve_count; i ++) {
    DBG("[MULTI] Sort curve #%d", i);
    Scenario::sortCandidates(scenarios[i], params, debug);
    DBG(" ");
  }

  if (debug) {
    logdebug("SORT> ## =candidate==================================================== E G dst final");
    for(int i = candidates.size() - 1; i >=0; i --) {
      MultiScenario *candidate = candidates[i];
      float score = candidate->getScore();
      int error_count = candidate->getErrorCount();
      int good_count = candidate->getGoodCount();
      int dist = (int) candidate->distance();
      QString name = candidate->getId();
      logdebug("SORT> %2d %62s %1d %1d%4d %5.2f",
	       i, QSTRING2PCHAR(name), error_count, good_count, dist, score);

    }
    logdebug("[MULTI] end dump");
    logdebug(" ");
  }
}

bool MultiScenario::nextLength(unsigned char next_letter, int curve_id, int &min_length, int &max_length) {
  if (curve_id < 0 || curve_id >= curve_count) { return false; }

  addSubScenarios();

  Scenario *scenario = S(curve_id);
  if (scenario->isFinished()) { return false; }

  return scenario->nextLength(next_letter, 0, min_length, max_length);
}

#endif /* MULTI */
