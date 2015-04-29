#include "incr_match.h"
#include "functions.h"
#include <iostream>
#include <cstdlib>

#ifdef INCREMENTAL

bool DelayedScenario::operator<(const DelayedScenario &other) const {
  return (this -> scenario < other.scenario);
}

DelayedScenario::DelayedScenario(Scenario &s, QHash<unsigned char, NextLetter> &h) : scenario(s), next(h) {
  dead = false;
}

NextLetter::NextLetter(LetterNode node, int min, int max) {
  letter_node = node;
  next_length_min = min;
  next_length_max = max;
}
NextLetter::NextLetter() {} // it makes QHash happy


void IncrementalMatch::displayDelayedScenario(DelayedScenario &ds) {
  QString txt;
  QTextStream ts(& txt);
  ts << "DelayedScenario[" << ds.scenario.getName() << "]: score=" << ds.scenario.getScore();
  foreach(unsigned char letter, ds.next.keys()) {
    ts << " " << QString(letter) << ":" << ds.next[letter].next_length_min << ":" << ds.next[letter].next_length_max;
  }
  DBG("%s", QSTRING2PCHAR(txt));
}

void IncrementalMatch::incrementalMatchBegin() {
  /* incremental algorithm: first iteration */
  scenarios.clear();
  candidates.clear();
  delayed_scenarios.clear();
 
  if (! loaded || ! keys.size()) { return; }

  quickKeys.setKeys(keys);
  quickCurve.setCurve(curve);

  Scenario root = Scenario(&wordtree, &quickKeys, &quickCurve, &params);
  root.setDebug(debug);
  scenarios.append(root);

  next_iteration_length = 0;
  last_curve_index = 0;

  memset(&st, 0, sizeof(st));

  QList<LetterNode> childNodes = root.getNextKeys();
  QHash<unsigned char, NextLetter> childs;
  foreach (LetterNode childNode, childNodes) {
    childs[childNode.getChar()] = NextLetter(childNode, 5, 5);
  }
  delayed_scenarios.append(DelayedScenario(root, childs));

  DBG("incrementalMatchBegin: delayed_scenarios=%d", delayed_scenarios.size());
}

void IncrementalMatch::aggressiveMatch() {
  incrementalMatchUpdate(false, true);
}

void IncrementalMatch::incrementalMatchUpdate(bool finished, bool aggressive) {
  /* incremental algorithm: subsequent iterations */
  if (! loaded || ! keys.size()) { return; }

  if (delayed_scenarios.size() == 0) { return; }
  if (cumulative_length < next_iteration_length && ! finished) { return; }  
  if (curve.size() < 5 && ! finished ) { return; }

  DBG("=== incrementalMatchUpdate: %sfinished=%d, curveIndex=%d, length=%d", aggressive?"[aggressive] ":"", finished, curve.size(), cumulative_length);
  QTime t_start = QTime::currentTime();

  next_iteration_length = -1;
 
  curvePreprocess1(last_curve_index);
  quickCurve.setCurve(curve); // curve may have new points since last iteration

  QList<DelayedScenario> new_delayed_scenarios;

  /* note: the incremental algorithm works in a single thread at the moment, but if less
     latency is needed, it can be made fully parallel by distributing the following loop
     over multiple cores.
     Not tried yet, as on the Jolla, this may slow down the GUI thread */

  int min_n = 0;
  foreach(DelayedScenario ds, delayed_scenarios) {
    if (ds.isDead()) { continue; }
    QList<unsigned char> allLetters = ds.next.keys();

    int n = ds.scenario.getCount();
    if (n < min_n || ! min_n) { min_n = n; }

    foreach(unsigned char letter, allLetters) {
      /* in "aggressive matching" try to evaluate child scenarios as soon as possible,
	 even it causes a lot of retries ... */

      int min_length = aggressive?ds.next[letter].next_length_min:ds.next[letter].next_length_max;

      if (cumulative_length >= min_length || finished) { 
	// now we can evaluate this scenario (and get next possible keys)

	LetterNode childNode = ds.next[letter].letter_node;
	if (evalChildScenario(ds.scenario, childNode, new_delayed_scenarios, finished)) {
	  ds.next.remove(letter);
	} else {
	  // retry later
	  ds.next[letter].next_length_min = cumulative_length + 50; // @todo parameter

	  if (ds.next[letter].next_length_min > ds.next[letter].next_length_max) {
	    ds.next[letter].next_length_max = ds.next[letter].next_length_min;
	  }
	}
      } else {
	update_next_iteration_length(min_length);
      }
    }

    if (ds.next.size() > 0) {
      new_delayed_scenarios.append(ds);
    }
  }
  delayed_scenarios = new_delayed_scenarios;
  last_curve_index = curve.size();

  delayedScenariosFilter();

  if (debug) {
    foreach(DelayedScenario ds, delayed_scenarios) { displayDelayedScenario(ds); }
  }

  if (finished) {
    /* this is the last iteration: compute final scores for candidates */
    curvePreprocess2();
    QList<Scenario> new_candidates;
    foreach(Scenario s, candidates) {
      if (s.postProcess()) {
	new_candidates.append(s);
      }
    }
    candidates = new_candidates;
    sortCandidates();
  }
  logdebug("=== incrementalMatchUpdate: %scurveIndex=%d, finished=%d, scenarios=%d, length=%d (next=%d), skim=%d, fork=%d, nodes=%d, retry=%d [time=%.3f]",
	   aggressive?"[aggressive] ":"", curve.size(), finished, delayed_scenarios.size(), cumulative_length, next_iteration_length, 
	   st.st_skim, st.st_fork, st.st_count, st.st_retry, (float)(t_start.msecsTo(QTime::currentTime())) / 1000);


  next_iteration_index = curve.size() + params.incremental_index_gap;
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
    
    int i = 0;
    foreach(DelayedScenario ds, delayed_scenarios) {
      scores[i ++] = ds.scenario.getTempScore();
    }
    std::qsort(scores, nb, sizeof(float), compareFloat);
    min_score = scores[nb - 1 - params.max_active_scenarios];

    delete[] scores;
  }
  
  QHash<QString, int> dejavu;

  for(int i = 0; i < delayed_scenarios.size(); i ++) {
    float sc = delayed_scenarios[i].scenario.getTempScore();    

    if (sc < min_score) {

      st.st_skim ++;
      delayed_scenarios[i].die();
      DBG("filtering(size): %s (%.3f)", delayed_scenarios[i].scenario.getNameCharPtr(), delayed_scenarios[i].scenario.getScore());

    } else if (! delayed_scenarios[i].scenario.forkLast()) {

      // remove scenarios with duplicate words (but not just after a scenario fork)
      QString name = delayed_scenarios[i].scenario.getName();
      if (dejavu.contains(name)) {
	int i0 = dejavu[name];
	float s0 = delayed_scenarios[i0].scenario.getTempScore();
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

void IncrementalMatch::update_next_iteration_length(int length) {
  /* records minimal curve length to trigger the next iteration */
  if (next_iteration_length <= 0 || length < next_iteration_length) {
    next_iteration_length = length;
  }
}


void IncrementalMatch::incrementalNextKeys(Scenario &scenario, QList<DelayedScenario> &result, bool finished) {
  /* create delayed scenarios, and compute length threshold for next iterations (for each possible key) */

  QHash<unsigned char, NextLetter> delayed_childs;

  QList<LetterNode> childNodes = scenario.getNextKeys();
  foreach (LetterNode childNode, childNodes) {

    unsigned char letter = childNode.getChar();
    Key k = keys[letter];
    int index = scenario.getCurveIndex();

    /*
     compute minimal curve length to evaluate this scenario
     (currently it's value is last key curve length + distance between the two keys + 2 * max distance error
     which means, that matching will always ~200 pixels late (with current settings)
     max_length is a pessimistic guess (it prevent retries and uses the less cpu time)
     min_length is an optimistic guess
    */
    Point keyPoint(k.x, k.y);
    int dist = distancep(curve[index], keyPoint);
    int max_length = finished?-1:(curve[index].length + (1.0 + (float)dist / params.dist_max_next / 20) * (params.incremental_length_lag + dist));
    int min_length = finished?-1:(curve[index].length + max(0, dist - params.incremental_length_lag / 2));

    bool keep_for_later = true;

    if (cumulative_length >= max_length || finished) {
      // if we can evaluate child scenario, imediately do it (and recurse into ourselves)
      if (evalChildScenario(scenario, childNode, result, finished)) {
	keep_for_later = false;
      } else {
	min_length = cumulative_length + 50;
      }
    }

    if (keep_for_later) {
      // keep it for a later iteration
      delayed_childs[letter] = NextLetter(childNode, min_length, max_length);
      update_next_iteration_length(max_length);
    }
    
  }

  if (delayed_childs.size() > 0) {
    // if some child scenario could not be evaluated now
    result.append(DelayedScenario(scenario, delayed_childs));
  }
}

bool IncrementalMatch::evalChildScenario(Scenario &scenario, LetterNode &childNode, QList<DelayedScenario> &result, bool finished) {
  /* evaluate a scenario (temporary score + create child scenario) using the standard (non-incremental) code */

  st.st_count += 1;
  if (childNode.hasPayload() && scenario.getCount() >= 1 && finished) {
    QList<Scenario> tmpList;
    scenario.childScenario(childNode, true, candidates, st.st_fork); // finished scenarios are directly added to candidates list
  }
  QList<Scenario> tmpList;
  if (scenario.childScenario(childNode, false, tmpList, st.st_fork, ! finished)) {
    foreach (Scenario scenario, tmpList) {
      incrementalNextKeys(scenario, result, finished);
    }
    return true;
  } else {
    // we are too close to the end of the curve, we'll have to retry later (only occurs when not finished)
    st.st_retry += 1;
    return false;
  }
}


void IncrementalMatch::clearCurve() {
  CurveMatch::clearCurve(); // parent
}

void IncrementalMatch::addPoint(Point point, int timestamp) {
  bool first_point = false;

  if (curve.size() == 0) {
    delayed_scenarios.clear();
    next_iteration_index = 0;
    timer.start();
    first_point = true;
  }

  DBG(" --- Add point #%d: (%d, %d)  length=%d", curve.size(), point.x, point.y, cumulative_length);
  CurveMatch::addPoint(point, timestamp); // parent

  cumulative_length = curve.last().length;

  if (first_point) {
    incrementalMatchBegin();
  } else if (curve.size() >= next_iteration_index) {
    incrementalMatchUpdate(false);
  }
  st.st_time = (int) timer.elapsed();
}

void IncrementalMatch::endCurve(int id) {
  incrementalMatchUpdate(true);
  done = true;
  
  CurveMatch::endCurve(id);
}

#endif /* INCREMENTAL */

