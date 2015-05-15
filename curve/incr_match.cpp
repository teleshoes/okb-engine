#include "incr_match.h"

#ifdef INCREMENTAL

#include <iostream>
#include <cstdlib>

#include "functions.h"

DelayedScenario::DelayedScenario(LetterTree *tree, QuickKeys *keys, QuickCurve *curves, Params *params) : scenario(tree, keys, curves, params) {
  dead = nextOk = false;
  debug = false;
};

DelayedScenario::DelayedScenario(const DelayedScenario &from) : scenario(from.scenario) {
  dead = false;
  next = from.next;
  nextOk = from.nextOk;
  debug = from.debug;
};

DelayedScenario::DelayedScenario(const ScenarioType &from) : scenario(from) {
  dead = nextOk = false;
  debug = from.debug;
}

DelayedScenario& DelayedScenario::operator=(const DelayedScenario &from) {
  this->scenario = from.scenario;
  this->dead = from.dead;
  this->next.clear();
  this->nextOk = false;
  this->debug = from.debug;

  return (*this);
};

DelayedScenario::~DelayedScenario() {
};


bool DelayedScenario::operator<(const DelayedScenario &other) const {
  return (this -> scenario.ScenarioType::operator<(other.scenario));
}

void DelayedScenario::updateNextLetters() {
  if (scenario.isFinished()) { return; }

  next.clear();

  foreach (LetterNode child, scenario.node.getChilds()) {
    unsigned char letter = child.getChar();
    next[letter] = NextLetter(child);
  }

  nextOk = true;
  updateNextLength();
}

void DelayedScenario::updateNextLength() {
  /* compute length threshold for next iterations (for each possible key) */

  if (! nextOk) { updateNextLetters(); return; }

  QHash<unsigned char, NextLetter>::iterator it;
  for(it = next.begin(); it != next.end(); it ++) {
    while (it.value().next_length.size() < 2 * scenario.curve_count) { it.value().next_length.append(0); }

    for(int curve_id = 0; curve_id < scenario.curve_count; curve_id ++) {
      unsigned char next_letter = it.key();
      int min_length, max_length;

      if (! scenario.nextLength(next_letter, curve_id, min_length, max_length)) {
	min_length = max_length = -1; // e.g. trying to add letter to finished sub-scenario
      }

      it.value().next_length[curve_id * 2] = min_length;
      it.value().next_length[curve_id * 2 + 1] = max_length;
    }

  }
}

void DelayedScenario::getChildsIncr(QList<DelayedScenario> &childs, bool curve_finished, stats_t &st, bool recursive, bool aggressive) {
  if (dead || scenario.finished) { return; }

  if (! nextOk) { updateNextLetters(); }

  QList<ScenarioType> tmpList;

  QHash<unsigned char, NextLetter>::iterator it = next.begin();
  while (it != next.end()) {
    LetterNode childNode = it.value().letter_node;
    bool flag_found = false, flag_wait = false;

    for (int curve_id = 0; curve_id < scenario.curve_count; curve_id ++) {
      /* in "aggressive matching" try to evaluate child scenarios as soon as possible,
	 even it causes a lot of retries ... */
      int nl_index = curve_id * 2 + (aggressive?0:1);
      int min_length = it.value().next_length[nl_index];
      int cur_length = scenario.curves[curve_id].getTotalLength();

      if (min_length == -1) {
	// no child possible for this curve. never retry

      } else if (cur_length > min_length || curve_finished) {
	st.st_count += 1;
	DBG("[INCR] Evaluate: %s + '%c' [curve_id=%d]", QSTRING2PCHAR(scenario.getId()), childNode.getChar(), curve_id);

	int last_size = tmpList.size();
	if (scenario.childScenario(childNode, tmpList, st.st_fork, curve_id, ! curve_finished)) {
	  // we've found all suitable child scenarios (possibly none at all)
	  if (tmpList.size() > last_size) {
	    // we've actually found a child --> no need to try the other curves
	    // (warning: this may cause problem in case of "finger collision")
	    flag_found = 1;
	  }

	} else {
	  // we are too close to the end of the curve, we'll have to retry later (only occurs when curve is not finished)
	  DBG("[INCR] Retry: %s + '%c'", QSTRING2PCHAR(scenario.getId()), childNode.getChar());
	  st.st_retry += 1;
	  it.value().next_length[nl_index] += scenario.params->incr_retry;
	  flag_wait = true;
	}

      } else {
	// Too noisy: DBG("[INCR] Waiting: %s + '%c'", QSTRING2PCHAR(scenario.getId()), childNode.getChar());
	flag_wait = true;
      }
    }
    if (flag_found || (! flag_wait)) {
      it = next.erase(it); // no more retry needed for childs with this letter
    } else {
      it ++;
    }
  }

  foreach(ScenarioType sc, tmpList) {
    DBG("[INCR] New scenario: %s", QSTRING2PCHAR(sc.getId()));
    DelayedScenario ds = DelayedScenario(sc);
    if (recursive && ! sc.isFinished()) {
      ds.getChildsIncr(childs, curve_finished, st, recursive, aggressive);
    }
    childs.append(ds);
  }

  if (! next.size()) { die(); } // we have explored all possible child scenarios
}


int DelayedScenario::getNextLength(int curve_id, bool aggressive) {
  if (! nextOk) { updateNextLetters(); }

  int next_length = 0;
  QHash<unsigned char, NextLetter>::iterator it;
  for(it = next.begin(); it != next.end(); it ++) {
    int length = it.value().next_length[curve_id * 2 + (aggressive?0:1) ];
    if (length > 0 && (length < next_length || ! next_length)) { next_length = length; }
  }
  return next_length;
}

void DelayedScenario::display(char *prefix) {
  if (! debug) { return; }

  QString txt;
  QTextStream ts(& txt);
  ts << "DelayedScenario(" << scenario.getId() << "): score=" << scenario.getScore();
  foreach(unsigned char letter, next.keys()) {
    ts << " " << QString(letter) << ":";
    bool first = true;
    foreach(int d, next[letter].next_length) {
      if (! first) { ts << ","; } else { first = false; }
      ts << d;
    }
    if (dead) { ts << " [DEAD]"; }
    if (scenario.finished) { ts << " [finished]"; }
  }
  DBG("%s%s", prefix?prefix:"", QSTRING2PCHAR(txt));
}


NextLetter::NextLetter(LetterNode node) {
  letter_node = node;
}
NextLetter::NextLetter() {} // it makes QHash happy



void IncrementalMatch::incrementalMatchBegin() {
  /* incremental algorithm: first iteration */
  delayed_scenarios.clear();
  candidates.clear();

  if (! loaded || ! keys.size()) { return; }

  quickKeys.setKeys(keys);
  setCurves();

  DelayedScenario root(&wordtree, &quickKeys, (QuickCurve*) &quickCurves, &params);
  root.setDebug(debug);

  memset(next_iteration_length, 0, sizeof(next_iteration_length));

  delayed_scenarios.append(root);

  DBG("incrementalMatchBegin: scenarios=%d", delayed_scenarios.size());

}

void IncrementalMatch::aggressiveMatch() {
  incrementalMatchUpdate(false, true);
}

QString IncrementalMatch::getLengthStr() {
  QString txt;
  QTextStream ts(& txt);
  for(int i = 0;i < curve_count; i ++) {
    ts << current_length[i] << "/" << next_iteration_length[i];
    if (i < curve_count - 1) { ts << " "; }
  }
  return txt;
}

void IncrementalMatch::incrementalMatchUpdate(bool finished, bool aggressive) {
  /* incremental algorithm: subsequent iterations */
  if (! loaded || ! keys.size()) { return; }

  if (delayed_scenarios.size() == 0) { return; }
  if (curve.size() < 5 && ! finished ) { return; }

  setCurves(); // curve may have new points since last iteration

  if (curve_count > last_curve_count) {
    // new touchpoint registered, user has just started a new curve
    DBG("New curve registered: #%d", curve_count);
    for(int i = 0; i < delayed_scenarios.size(); i ++) {
      delayed_scenarios[i].scenario.setCurveCount(curve_count);
      delayed_scenarios[i].updateNextLength();
    }
    last_curve_count = curve_count;
  }

  bool proceed = false;
  for(int i = 0; i < curve_count; i ++) {
    current_length[i] = quickCurves[i].getTotalLength();
    if (current_length[i] >= next_iteration_length[i]) { proceed = true; }
  }

  logdebug("[== incrementalMatchUpdate: %sfinished=%d, curveIndex=%d, length=[%s], proceed=%d", aggressive?"[aggressive] ":"", finished, curve.size(), QSTRING2PCHAR(getLengthStr()), proceed);

  if (debug) {
    for(int i = 0; i < delayed_scenarios.size(); i ++) {
      DelayedScenario *ds = &(delayed_scenarios[i]);
      if (debug) { ds->display((char*) "dump DS> "); }
    }
  }

  next_iteration_index = curve.size() + params.incremental_index_gap;

  if ((! proceed) && (! finished)) { return; }

  QTime t_start = QTime::currentTime();

  memset(next_iteration_length, 0, sizeof(next_iteration_length));

  QList<DelayedScenario> new_delayed_scenarios;

  /* note: the incremental algorithm works in a single thread at the moment, but if less
     latency is needed, it can be made fully parallel by distributing the following loop
     over multiple cores.
     Not tried yet, as on the Jolla, this may slow down the GUI thread */

  for(int i = 0; i < delayed_scenarios.size(); i ++) {
    DelayedScenario *ds = &(delayed_scenarios[i]);
    if (debug) { ds->display((char*) "DS> "); }
    if (ds->dead) { continue; }

    ds->getChildsIncr(new_delayed_scenarios, finished, st, true, aggressive); // getChildsIncr will fail fast if curves length are not high enough
    if (! ds->dead) { new_delayed_scenarios.append(*ds); } // DelayedScenarios will "die" when all their possible childs has been created
  }

  // copy back unfinished scenarios to scenario list
  delayed_scenarios.clear();
  for(int i = 0 ; i < new_delayed_scenarios.size(); i ++) {
    if (new_delayed_scenarios[i].scenario.isFinished()) {
      candidates.append(new_delayed_scenarios[i].scenario); // add to candidate list

    } else {
      delayed_scenarios.append(new_delayed_scenarios[i]);
    }
  }

  delayedScenariosFilter();

  update_next_iteration_length(aggressive);

  if (finished) {
    /* this is the last iteration: compute final scores for candidates */
    curvePreprocess2();
    QList<ScenarioType> new_candidates;
    foreach(ScenarioType s, candidates) {
      if (s.postProcess()) {
	new_candidates.append(s);
      }
    }
    candidates = new_candidates;

    scenarioFilter(candidates, 0.5, 10, 3 * params.max_candidates, true); // @todo add to parameter list
    sortCandidates();
    scenarioFilter(candidates, 0.7, 10, params.max_candidates, true); // @todo add to parameter list
  }
  logdebug("==] incrementalMatchUpdate: %scurveIndex=%d, finished=%d, scenarios=%d, skim=%d, fork=%d, nodes=%d, retry=%d [time=%.3f]",
	   aggressive?"[aggressive] ":"", curve.size(), finished, delayed_scenarios.size(),
	   st.st_skim, st.st_fork, st.st_count, st.st_retry, (float)(t_start.msecsTo(QTime::currentTime())) / 1000);
}

void IncrementalMatch::update_next_iteration_length(bool aggressive) {
  /* records minimal curve length to trigger the next iteration */
  memset(next_iteration_length, 0, sizeof(next_iteration_length));

  for(int i = 0; i < delayed_scenarios.size(); i ++) {
    DelayedScenario *ds = &(delayed_scenarios[i]);

    for(int j = 0; j < curve_count; j ++) {
      int next_length = ds->getNextLength(j, aggressive);
      if ((next_length > 0) && (next_length < next_iteration_length[j] || ! next_iteration_length[j])) {
	next_iteration_length[j] = next_length;
      }
    }
  }
}

static int compareFloat (const void * a, const void * b)
{
  float a1 = *(float*)a;
  float b1 = *(float*)b;

  return (a1 > b1) - (a1 < b1);
}

void IncrementalMatch::delayedScenariosFilter() {
  /* makes sure thats delayed scenario list stays at a reasoneable size & remove duplicate
     This is an adaptation from scenarioFilter() in curve_match.cpp */

  DBG("Scenarios filter ...");

  int nb = delayed_scenarios.size();
  float min_score = 0;
  if (nb > params.max_active_scenarios) {
    float* scores = new float[nb];

    for(int i = 0; i < delayed_scenarios.size(); i ++) {
      scores[i] = delayed_scenarios[i].scenario.getScore();
    }
    std::qsort(scores, nb, sizeof(float), compareFloat); // wtf ???
    min_score = scores[nb - 1 - params.max_active_scenarios];

    delete[] scores;
  }

  QHash<QString, int> dejavu;

  for(int i = 0; i < delayed_scenarios.size(); i ++) {
    float sc = delayed_scenarios[i].scenario.getScore();

    if (sc < min_score) {

      st.st_skim ++;
      delayed_scenarios[i].die();
      DBG("filtering(size): %s (%.3f)", delayed_scenarios[i].scenario.getNameCharPtr(), delayed_scenarios[i].scenario.getScore());

    } else if (! delayed_scenarios[i].scenario.forkLast()) {

      // remove scenarios with duplicate words (but not just after a scenario fork)
      QString name = delayed_scenarios[i].scenario.getName();
      if (dejavu.contains(name)) {
	int i0 = dejavu[name];
	float s0 = delayed_scenarios[i0].scenario.getScore();
	if (sc > s0) {
	  delayed_scenarios[i0].die();
	  dejavu.insert(name,i);
	} else {
	  delayed_scenarios[i].die();
	}
	continue; // retry iteration
      } else {
	if (! delayed_scenarios[i].scenario.forkLast()) {
	  dejavu.insert(name, i);
	}
      }

    }

  }

}


void IncrementalMatch::addPoint(Point point, int curve_id, int timestamp) {
  bool first_point = false;

  if (curve.size() == 0) {
    curve_count = 0;
    delayed_scenarios.clear();
    next_iteration_index = 0;
    timer.start();
    first_point = true;
    last_curve_count = 0;
  }

  DBG(" --- Add point #%d: (%d, %d)", curve.size(), point.x, point.y);
  CurveMatch::addPoint(point, curve_id, timestamp); // parent

  if (first_point) {
    incrementalMatchBegin();
  } else if (curve.size() >= next_iteration_index) {
    incrementalMatchUpdate(false);
  }
  st.st_time = (int) timer.elapsed();
}

void IncrementalMatch::endOneCurve(int curve_id) {
  CurveMatch::endOneCurve(curve_id);
}

void IncrementalMatch::endCurve(int id) {
  done = true;
  CurveMatch::endCurve(id);
  incrementalMatchUpdate(true);
}

#endif /* INCREMENTAL */

