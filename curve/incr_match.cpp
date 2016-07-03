#include "incr_match.h"

#ifdef INCREMENTAL

#include <iostream>
#include <cstdlib>

#include "functions.h"

#define SC_METHOD(method, ...) (multi?(multi_p.data()->method(__VA_ARGS__)):(single_p.data()->method(__VA_ARGS__)))
#define SC_PROP(prop) (multi?(multi_p.data()->prop):(single_p.data()->prop))

DelayedScenario::DelayedScenario(LetterTree *tree, QuickKeys *keys, QuickCurve *curves, Params *params) {
  dead = nextOk = false;
  debug = false;

  multi = false;
  single_p.reset(new Scenario(tree, keys, curves, params));

  this -> params = params;
  curve_count = 0;

  MultiScenario::init(); // static context -> bad
};

DelayedScenario::DelayedScenario(const DelayedScenario &from) {
  dead = false;
  next = from.next;
  nextOk = from.nextOk;
  debug = from.debug;

  multi = from.multi;
  multi_p = from.multi_p;
  single_p = from.single_p;
  params = from.params;
  curve_count = from.curve_count;
}

DelayedScenario::DelayedScenario(const MultiScenario &from) {
  dead = nextOk = false;
  debug = from.debug;

  multi = true;
  multi_p.reset(new MultiScenario(from));

  params = from.params;
  curve_count = multi_p.data() -> curve_count;
}

DelayedScenario::DelayedScenario(const Scenario &from) {
  dead = nextOk = false;
  debug = from.debug;

  multi = false;
  single_p.reset(new Scenario(from));

  params = from.params;
  curve_count = 1;
}

DelayedScenario& DelayedScenario::operator=(const DelayedScenario &from) {
  this->multi_p = from.multi_p;
  this->single_p = from.single_p;
  this->dead = from.dead;
  this->next.clear();
  this->nextOk = false;
  this->debug = from.debug;

  this->params = from.params;
  this->multi = from.multi;
  this->curve_count = from.curve_count;

  return (*this);
};

DelayedScenario::~DelayedScenario() {
  // nothing to do: smart pointers are smart :)
};


bool DelayedScenario::operator<(const DelayedScenario &other) const {
  if (multi) {
    return this -> multi_p.data() -> operator<(*(other.multi_p.data()));
  } else {
    return this -> single_p.data() -> operator<(*(other.single_p.data()));
  }
}

bool DelayedScenario::isFinished() {
  return SC_METHOD(isFinished);
}

void DelayedScenario::setDebug(bool value) {
  SC_METHOD(setDebug, value);
  this -> debug = value;
}

void DelayedScenario::setCurveCount(int count) {
  if (count > 1 && ! multi) {
    multi = true;
    this -> multi_p.reset(new MultiScenario(*(this -> single_p.data())));
    this -> single_p.clear();
  }
  SC_METHOD(setCurveCount, count);
  curve_count = count;
}

bool DelayedScenario::nextLength(unsigned char next_letter, int curve_id, int &min_length, int &max_length) {
  return SC_METHOD(nextLength, next_letter, curve_id, min_length, max_length);
}

int DelayedScenario::getTotalLength(int curve_id) {
  if (multi) {
    return this -> multi_p.data() -> curves[curve_id].getTotalLength();
  } else {
    return this -> single_p.data() -> curve -> getTotalLength();
  }
}

LetterNode DelayedScenario::getNode() {
  return SC_PROP(node);
}

QString DelayedScenario::getId() {
  return SC_METHOD(getId);
}

float DelayedScenario::getScore() {
  return SC_METHOD(getScore);
}

QString DelayedScenario::getWordList() {
  return SC_METHOD(getWordList);
}

MultiScenario DelayedScenario::getMultiScenario() {
  if (multi) {
    return MultiScenario(*(this -> multi_p.data()));
  } else {
    return MultiScenario(*(this -> single_p.data()));
  }
}

int DelayedScenario::getCount() {
  return SC_METHOD(getCount);
}

int DelayedScenario::forkLast() {
  return SC_METHOD(forkLast);
}

QString DelayedScenario::getName() {
  return SC_METHOD(getName);
}

void DelayedScenario::updateNextLetters() {
  if (isFinished()) { return; }

  next.clear();

  foreach (LetterNode child, getNode().getChilds()) {
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
    while (it.value().next_length.size() < 2 * curve_count) { it.value().next_length.append(0); }

    for(int curve_id = 0; curve_id < curve_count; curve_id ++) {
      unsigned char next_letter = it.key();
      int min_length = 0, max_length = 0;

      if (! nextLength(next_letter, curve_id, min_length, max_length)) {
	min_length = max_length = -1; // e.g. trying to add letter to finished sub-scenario
      }

      it.value().next_length[curve_id * 2] = min_length;
      it.value().next_length[curve_id * 2 + 1] = max_length;
    }

  }
}

void DelayedScenario::getChildsIncr(QList<DelayedScenario> &childs, bool curve_finished, stats_t &st, bool recursive, float aggressive) {
  if (dead || isFinished()) { return; }

  if (! nextOk) { updateNextLetters(); }

  QList<MultiScenario> tmpListM;
  QList<Scenario> tmpListS;

  QHash<unsigned char, NextLetter>::iterator it = next.begin();
  while (it != next.end()) {
    LetterNode childNode = it.value().letter_node;
    bool flag_found = false, flag_wait = false;

    for (int curve_id = 0; curve_id < curve_count; curve_id ++) {
      /* in "aggressive matching" try to evaluate child scenarios as soon as possible,
	 even it causes a lot of retries ... */
      int nl_index = curve_id * 2;
      int min_length = aggressive * it.value().next_length[nl_index] + (1 - aggressive) * it.value().next_length[nl_index + 1];
      int cur_length = getTotalLength(curve_id);

      if (min_length == -1) {
	// no child possible for this curve. never retry

      } else if (cur_length > min_length || curve_finished) {
	st.st_count += 1;
	DBG("[INCR] Evaluate: %s + '%c' [curve_id=%d]", QSTRING2PCHAR(getId()), childNode.getChar(), curve_id);

	int last_size = tmpListM.size() + tmpListS.size();
	bool result;
	if (multi) {
	  result = multi_p.data()->childScenario(childNode, tmpListM, st, curve_id, ! curve_finished);
	} else {
	  result = single_p.data()->childScenario(childNode, tmpListS, st, 0, ! curve_finished);
	}
	if (result) {
	  // we've found all suitable child scenarios (possibly none at all)
	  if (tmpListM.size() + tmpListS.size() > last_size) {
	    // we've actually found a child --> no need to try the other curves
	    // (warning: this may cause problem in case of "finger collision")
	    flag_found = 1;
	  }

	} else {
	  // we are too close to the end of the curve, we'll have to retry later (only occurs when curve is not finished)
	  DBG("[INCR] Retry: %s + '%c'", QSTRING2PCHAR(getId()), childNode.getChar());
	  st.st_retry += 1;
	  it.value().next_length[nl_index]     += params->incr_retry;
	  it.value().next_length[nl_index + 1] += params->incr_retry;
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

  QList<DelayedScenario> new_ds;

  if (multi) {
    foreach(MultiScenario sc, tmpListM) {
      DBG("[INCR] New scenario: %s", QSTRING2PCHAR(sc.getId()));
      DelayedScenario ds = DelayedScenario(sc);
      if (recursive && ! sc.isFinished()) {
	ds.getChildsIncr(childs, curve_finished, st, recursive, aggressive);
      }
      childs.append(ds);
    }
  } else {
    foreach(Scenario sc, tmpListS) {
      DBG("[INCR] New scenario: %s", QSTRING2PCHAR(sc.getId()));
      DelayedScenario ds = DelayedScenario(sc);
      if (recursive && ! sc.isFinished()) {
	ds.getChildsIncr(childs, curve_finished, st, recursive, aggressive);
      }
      childs.append(ds);
    }
  }

  if (! next.size()) { die(); } // we have explored all possible child scenarios
}


int DelayedScenario::getNextLength(int curve_id, float aggressive) {
  if (! nextOk) { updateNextLetters(); }

  int next_length = 0;
  QHash<unsigned char, NextLetter>::iterator it;
  for(it = next.begin(); it != next.end(); it ++) {
    int length = aggressive * it.value().next_length[curve_id * 2] + (1 - aggressive) * it.value().next_length[curve_id * 2 + 1];
    if (length > 0 && (length < next_length || ! next_length)) { next_length = length; }
  }
  return next_length;
}

void DelayedScenario::display(char *prefix) {
  if (! debug) { return; }

  QString txt;
  QTextStream ts(& txt);
  ts << "DelayedScenario(" << getId() << "): score=" << getScore();
  if (dead) { ts << " [DEAD]"; }
  if (isFinished()) { ts << " [finished]"; }
  foreach(unsigned char letter, next.keys()) {
    ts << " " << QString(letter) << ":";
    bool first = true;
    foreach(int d, next[letter].next_length) {
      if (! first) { ts << ","; } else { first = false; }
      ts << d;
    }
  }
  DBG("%s%s", prefix?prefix:"", QSTRING2PCHAR(txt));
}

void DelayedScenario::deepDive(QList<Scenario> &result) {
  if (multi) { return; } // not supported at the moment

  single_p.data()->deepDive(result);
}


NextLetter::NextLetter(LetterNode node) {
  letter_node = node;
}
NextLetter::NextLetter() {} // it makes QHash happy


IncrementalMatch::IncrementalMatch() {
  delayed_scenarios_p = new QList<DelayedScenario>();
}

IncrementalMatch::~IncrementalMatch() {
  delete delayed_scenarios_p;
}

void IncrementalMatch::purge_snapshots() {
  DBG("[purge snapshots: %d]", ds_snapshots.size());
  for(int i = 0;i < ds_snapshots.size(); i++) {
    delete ds_snapshots[i];
  }
  ds_snapshots.clear();
}

#define delayed_scenarios (*delayed_scenarios_p)

void IncrementalMatch::incrementalMatchBegin() {
  /* incremental algorithm: first iteration */
  delayed_scenarios.clear();
  candidates.clear();
  purge_snapshots();

  if (! loaded || ! keys.size()) { return; }

  quickKeys.setKeys(keys);
  setCurves();

  DelayedScenario root(&wordtree, &quickKeys, (QuickCurve*) &quickCurves, &params);
  root.setDebug(debug);
  root.setCurveCount(curve_count);

  memset(next_iteration_length, 0, sizeof(next_iteration_length));

  delayed_scenarios.append(root);

  last_snapshot_count = 0;

  DBG("incrementalMatchBegin: scenarios=%d", delayed_scenarios.size());

}

void IncrementalMatch::aggressiveMatch() {
  incrementalMatchUpdate(false, 1.0);
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

void IncrementalMatch::incrementalMatchUpdate(bool finished, float aggressive) {
  /* incremental algorithm: subsequent iterations */
  if (! loaded || ! keys.size()) { return; }

  if (delayed_scenarios.size() == 0 && ! finished) { return; }
  if (curve.size() < 5 && ! finished ) { return; }

  setCurves(); // curve may have new points since last iteration

  if (curve_count > last_curve_count) {
    // new touchpoint registered, user has just started a new curve
    DBG("New curve registered: #%d", curve_count);
    for(int i = 0; i < delayed_scenarios.size(); i ++) {
      delayed_scenarios[i].setCurveCount(curve_count);
      delayed_scenarios[i].updateNextLength();
    }
    last_curve_count = curve_count;
  }

  bool proceed = false;
  for(int i = 0; i < curve_count; i ++) {
    current_length[i] = quickCurves[i].getTotalLength();
    if (current_length[i] >= next_iteration_length[i]) { proceed = true; }
  }

  logdebug("[== incrementalMatchUpdate: finished=%d, curveIndex=%d, length=[%s], proceed=%d aggressive=%.2f", finished, curve.size(), QSTRING2PCHAR(getLengthStr()), proceed, aggressive);

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

  QList<DelayedScenario> *new_delayed_scenarios_p = new QList<DelayedScenario>();

  /* note: the incremental algorithm works in a single thread at the moment, but if less
     latency is needed, it can be made fully parallel by distributing the following loop
     over multiple cores.
     Not tried yet, as on the Jolla, this may slow down the GUI thread */

  for(int i = 0; i < delayed_scenarios.size(); i ++) {
    DelayedScenario *ds = &(delayed_scenarios[i]);
    if (debug) { ds->display((char*) "DS> "); }
    if (ds->dead) { continue; }

    ds->getChildsIncr(*new_delayed_scenarios_p, finished, st, true, aggressive); // getChildsIncr will fail fast if curves length are not high enough
    if (ds->dead) { continue; } // DelayedScenarios will "die" when all their possible childs has been created

    new_delayed_scenarios_p->append(*ds);
  }

  // find candidates
  int c_total = 0, c_count = 0;
  for(int i = 0 ; i < new_delayed_scenarios_p->size(); i ++) {
    if ((*new_delayed_scenarios_p)[i].isFinished()) {
      if ((*new_delayed_scenarios_p)[i].getWordList().size()) {
	// This test is a workaround for a real bug (@todo fix this)
	candidates.append((*new_delayed_scenarios_p)[i].getMultiScenario()); // add to candidate list
      }
      (*new_delayed_scenarios_p)[i].die();
    } else {
      c_total += (*new_delayed_scenarios_p)[i].getCount();
      c_count ++;
    }
  }

  // flip pointer from old to new list (and avoid a double copy)
  QList<DelayedScenario> *old_dsp = delayed_scenarios_p;
  delayed_scenarios_p = new_delayed_scenarios_p;

  // keep snapshot for later backtracking with the fallback algorithm
  int avg_count = c_total / (c_count?c_count:1);
  if (avg_count >= params.fallback_start_count && avg_count > last_snapshot_count && delayed_scenarios.size() > 0) {
    DBG("[Taking snapshot: %d, size: %d]", avg_count, old_dsp->size());
    last_snapshot_count = avg_count;
    ds_snapshots.append(old_dsp);
    if (ds_snapshots.size() > params.fallback_snapshot_queue) { delete ds_snapshots.takeFirst(); }
  } else {
    delete old_dsp;
  }

  delayedScenariosFilter();

  update_next_iteration_length(aggressive);

  if (finished) {
    /* this is the last iteration: compute final scores for candidates */

    bool fallback_done = false;
    if (candidates.size() == 0) {
      // oops, we did not find any good match, try with fallback
      fallback(candidates);
      fallback_done = true;
    }

    purge_snapshots();

    curvePreprocess2();
    QList<ScenarioType> new_candidates;
    foreach(ScenarioType s, candidates) {
      if (s.postProcess(st)) {
	new_candidates.append(s);
      }
    }
    candidates = new_candidates;

    int max_candidates = fallback_done?params.fallback_max_candidates:params.max_candidates;

    scenarioFilter(candidates, 0.5, 10, 3 * max_candidates, true); // @todo add to parameter list
    sortCandidates();
    scenarioFilter(candidates, 0.7, 10, max_candidates, true); // @todo add to parameter list

    // cache stats
    int n = st.st_cache_hit;
    int d = st.st_cache_hit + st.st_cache_miss;
    if (d) {
      logdebug("Cache stats: access count: %d, hit ratio: %.2f%%", d, 100.0 * n / d);
    }

  }
  logdebug("==] incrementalMatchUpdate: curveIndex=%d, finished=%d, scenarios=%d, skim=%d, fork=%d, nodes=%d, retry=%d [time=%.3f]",
	   curve.size(), finished, delayed_scenarios.size(),
	   st.st_skim, st.st_fork, st.st_count, st.st_retry, (float)(t_start.msecsTo(QTime::currentTime())) / 1000);
}

void IncrementalMatch::fallback(QList<ScenarioType> &result) {
  if (ds_snapshots.size() == 0) { return; }
  QList<DelayedScenario> *ds_list = ds_snapshots[0];
  logdebug("[fallback, size: %d]", ds_list->size());

  if (ds_list->size() == 0) { return; }

  QSet<QString> dedupe;
  for(int i = ds_list->size() - 1; i >= 0; i --) {
    QString name = (*ds_list)[i].getName();
    if (name.length() < params.fallback_min_length) { continue; }
    dedupe.insert(name);
  }

  qSort(ds_list->begin(), ds_list->end());

  QTime startTime = QTime::currentTime();

  QList<Scenario> list;
  for(int i = ds_list->size() - 1; i >= 0; i --) {
    QString name = (*ds_list)[i].getName();
    if (name.length() < params.fallback_min_length) { continue; }

    QString parent = name.left(name.length() - 1);
    if (dedupe.contains(parent)) { continue; }

    (*ds_list)[i].deepDive(list);
    if (list.size() > params.fallback_max_candidates * 4) {
      qSort(list.begin(), list.end());
      DBG("[fallback: trimming list (%d) ]", list.size());
      list = list.mid(list.size() - params.fallback_max_candidates * 3);
    }

    QTime now = QTime::currentTime();

    if (startTime.msecsTo(now) > params.fallback_timeout) {
      // timebox fallback time
      // during tests it was always acceptable, but there is no upper bound on
      // time required to process all possible scenarios, so let's stay on the safe side
      logdebug("[fallbak: TIMEOUT!]");
      break;
    }

  }
  foreach(Scenario s, list) {
    result.append(MultiScenario(s));
  }
}

void IncrementalMatch::update_next_iteration_length(float aggressive) {
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
  float min_score = 0, min_score2 = 0;
  if (nb > params.max_active_scenarios) {
    float* scores = new float[nb];

    for(int i = 0; i < delayed_scenarios.size(); i ++) {
      scores[i] = delayed_scenarios[i].getScore();
    }
    std::qsort(scores, nb, sizeof(float), compareFloat); // wtf ???
    min_score = scores[nb - 1 - params.max_active_scenarios];
    if (nb > params.max_active_scenarios2) {
      min_score2 = scores[nb - 1 - params.max_active_scenarios2];
    }

    delete[] scores;
  }

  QHash<QString, int> dejavu;

  for(int i = 0; i < delayed_scenarios.size(); i ++) {
    float sc = delayed_scenarios[i].getScore();

    float compare = delayed_scenarios[i].forkLast()?min_score2:min_score;

    if (sc < compare && delayed_scenarios[i].getCount() > 2) {

      st.st_skim ++;
      delayed_scenarios[i].die();
      DBG("filtering(size): %s (%.3f)", QSTRING2PCHAR(delayed_scenarios[i].getId()), delayed_scenarios[i].getScore());

    } else if (! delayed_scenarios[i].forkLast()) {

      // remove scenarios with duplicate words (but not just after a scenario fork)
      QString name = delayed_scenarios[i].getName();
      if (dejavu.contains(name)) {
	int i0 = dejavu[name];
	float s0 = delayed_scenarios[i0].getScore();
	if (sc > s0) {
	  delayed_scenarios[i0].die();
	  DBG("filtering(fork1): %s (%.3f)", QSTRING2PCHAR(delayed_scenarios[i0].getId()), delayed_scenarios[i0].getScore());
	  dejavu.insert(name,i);
	} else {
	  delayed_scenarios[i].die();
	  DBG("filtering(fork2): %s (%.3f)", QSTRING2PCHAR(delayed_scenarios[i].getId()), delayed_scenarios[i].getScore());
	}
	continue; // retry iteration
      } else {
	if (! delayed_scenarios[i].forkLast()) {
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
    start_cpu_time = getCPUTime();
    first_point = true;
    last_curve_count = 0;
  }

  DBG(" --- Add point #%d: (%d, %d)", curve.size(), point.x, point.y);
  CurveMatch::addPoint(point, curve_id, timestamp); // parent

  if (first_point) {
    incrementalMatchBegin();
  } else if (curve.size() >= next_iteration_index) {
    incrementalMatchUpdate(false, params.aggressive_mode);
  }
  st.st_cputime = (int) (1000 * (getCPUTime() - start_cpu_time));
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

