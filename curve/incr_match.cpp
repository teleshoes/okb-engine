#include "incr_match.h"
#include "functions.h"

#include <QDebug>

bool DelayedScenario::operator<(const DelayedScenario &other) const {
  return (this -> scenario < other.scenario);
}

DelayedScenario::DelayedScenario(Scenario &s, QHash<unsigned char, QPair<LetterNode, int> > &h) : scenario(s), next(h) {}



void IncrementalMatch::displayDelayedScenario(DelayedScenario &ds) {
  QString txt;
  QTextStream ts(& txt);
  ts << "DelayedScenario[" << ds.scenario.getName() << "]:";
  foreach(unsigned char letter, ds.next.keys()) {
    ts << " " << QString(letter) << ":" << ds.next[letter].second;
  }
  DBG("%s", txt.toLocal8Bit().constData());
}

void IncrementalMatch::incrementalMatchBegin() {
  /* incremental algorithm: first iteration */
  scenarios.clear();
  candidates.clear();
  delayed_scenarios.clear();
  index2distance.clear();
 
  if (! loaded || ! keys.size()) { return; }

  Scenario root = Scenario(&wordtree, &keys, &curve, &params);
  root.setDebug(debug);
  scenarios.append(root);

  cumulative_length = 0;
  next_iteration_length = 0;
  last_curve_index = 0;
  next_iteration_index = 0;

  st_fork = 0;
  st_skim = 0;
  st_count = 0;

  QList<LetterNode> childNodes = root.getNextKeys();
  QHash<unsigned char, QPair<LetterNode, int> > childs;
  foreach (LetterNode childNode, childNodes) {
    childs[childNode.getChar()] = QPair<LetterNode, int>(childNode, 5);
  }
  delayed_scenarios.append(DelayedScenario(root, childs));

  DBG("incrementalMatchBegin: delayed_scenarios=%d", delayed_scenarios.size());
}

void IncrementalMatch::incrementalMatchUpdate(bool finished) {
  /* incremental algorithm: subsequent iterations */
  if (! loaded || ! keys.size()) { return; }

  if (delayed_scenarios.size() == 0) { return; }
  if (cumulative_length < next_iteration_length && ! finished) { return; }  
  if (curve.size() < 5 && ! finished ) { return; }
  if (curve.size() < next_iteration_index && ! finished) { return; }

  DBG("[- incrementalMatchUpdate: finished=%d, curveIndex=%d, length=%d", finished, curve.size(), cumulative_length);

  next_iteration_length = -1;
 
  curvePreprocess1(last_curve_index);

  QList<DelayedScenario> new_delayed_scenarios;

  int min_n = 0;
  foreach(DelayedScenario ds, delayed_scenarios) {
    QList<unsigned char> allLetters = ds.next.keys();

    int n = ds.scenario.getCount();
    if (n < min_n || ! min_n) { min_n = n; }

    foreach(unsigned char letter, allLetters) {
      int min_length = ds.next[letter].second;

      if (cumulative_length >= min_length || finished) {
	// now we can evaluate this scenario (and get next possible keys)
	LetterNode childNode = ds.next[letter].first;
	evalChildScenario(ds.scenario, childNode, new_delayed_scenarios, finished);
	ds.next.remove(letter);
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

  if (min_n >= 2) {
    delayedScenariosFilter();
  }

  if (debug) {
    foreach(DelayedScenario ds, delayed_scenarios) { displayDelayedScenario(ds); }
  }

  if (finished) {
    /* this is the last iteration: compute final scores for candidates */
    curvePreprocess2();
    QList<Scenario> new_candidates;
    foreach(Scenario s, candidates) {
      s.postProcess();
      new_candidates.append(s);
    }
    candidates = new_candidates;
  }
  DBG("-] incrementalMatchUpdate: curveIndex=%d, finished=%d, scenarios=%d, length=%d (next=%d), skim=%d, fork=%d, nodes=%d",
      curve.size(), finished, delayed_scenarios.size(), cumulative_length, next_iteration_length, st_skim, st_fork, st_count);


  next_iteration_index = curve.size() + 8;
}

void IncrementalMatch::delayedScenariosFilter() {
  /* makes sure thats delayed scenario list stays at a reasoneable size & remove duplicate
   This is an adaptation from scenarioFilter() in curve_match.cpp */

  DBG("Scenarios filter ...");

  qSort(delayed_scenarios.begin(), delayed_scenarios.end());

  QHash<QString, int> dejavu;

  int i = 0;
  while (i < delayed_scenarios.size()) {
    float sc = delayed_scenarios[i].scenario.getTempScore();

    // remove scenarios with duplicate words (but not just after a scenario fork)
    if (! delayed_scenarios[i].scenario.forkLast()) {
      QString name = delayed_scenarios[i].scenario.getName();
      if (dejavu.contains(name)) {
	int i0 = dejavu[name];
	float s0 = delayed_scenarios[i0].scenario.getTempScore();
	if (sc > s0) {
	  delayed_scenarios.takeAt(i0);
	  foreach(QString n, dejavu.keys()) {
	    if (dejavu[n] > i0 && dejavu[n] < i) {
	      dejavu[n] --;
	    }
	  }
	  i --;
	  dejavu.remove(name); // will be added again on next iteration
	} else {
	  delayed_scenarios.takeAt(i);	  
	}
	continue; // retry iteration
      } else {
	dejavu.insert(name, i);
      }
    }

    i++;
  }

  // enforce max scenarios count
  while (delayed_scenarios.size() > params.max_active_scenarios) {
    st_skim ++;
    Scenario s = delayed_scenarios.takeAt(0).scenario;
    DBG("filtering(size): %s (%.3f)", s.getName().toLocal8Bit().constData(), s.getScore());
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

  QHash<unsigned char, QPair<LetterNode, int> > delayed_childs;

  QList<LetterNode> childNodes = scenario.getNextKeys();
  foreach (LetterNode childNode, childNodes) {

    unsigned char letter = childNode.getChar();
    Key k = keys[letter];
    int index = scenario.getCurveIndex();

    /*
     compute minimal curve length to evaluate this scenario
     (currently it's value is last key curve length + distance between the two keys + 2 * max distance error
     which means, that matching will always ~200 pixels late (with current settings)
     @todo use more optimistic formula + retries in case we try too soon
       -> add a new "call me again later" return value to get_next_key_match()
    */
    Point keyPoint(k.x, k.y);
    int min_length = finished?-1:(index2distance[index] + 2 * params.dist_max_next + distancep(curve[index], keyPoint));

    if (cumulative_length >= min_length || finished) {
      // if we can evaluate child scenario, imediately do it (and recurse into ourselves)
      evalChildScenario(scenario, childNode, result, finished);
    } else {
      // keep it for a later iteration
      delayed_childs[letter] = QPair<LetterNode, int>(childNode, min_length);
      update_next_iteration_length(min_length);
    }
    
  }

  if (delayed_childs.size() > 0) {
    // if some child scenario could not be evaluated now
    result.append(DelayedScenario(scenario, delayed_childs));
  }
}

void IncrementalMatch::evalChildScenario(Scenario &scenario, LetterNode &childNode, QList<DelayedScenario> &result, bool finished) {
  /* evaluate a scenario (temporary score + create child scenario) using the standard (non-incremental) code */

  st_count += 1;
  if (childNode.hasPayload() && scenario.getCount() >= 1 && finished) {
    QList<Scenario> tmpList;
    scenario.childScenario(childNode, true, candidates, st_fork); // finished scenarios are directly added to candidates list
  }
  QList<Scenario> tmpList;
  scenario.childScenario(childNode, false, tmpList, st_fork);
  foreach (Scenario scenario, tmpList) {
    incrementalNextKeys(scenario, result, finished);
  }
}


void IncrementalMatch::clearCurve() {
  CurveMatch::clearCurve(); // parent
}

void IncrementalMatch::addPoint(Point point) {
  if (curve.size() > 0) {
      cumulative_length += distancep(point, curve.last());
  }
  DBG(" --- Add point #%d: (%d, %d)  length=%d", curve.size(), point.x, point.y, cumulative_length);
  CurveMatch::addPoint(point); // parent
  index2distance.append(cumulative_length);
  if (curve.size() == 1) {
    incrementalMatchBegin();
  } else {
    incrementalMatchUpdate(false);
  }
}

void IncrementalMatch::endCurve(int id) {
  incrementalMatchUpdate(true);
  done = true;
  
  scenarioFilter(candidates, 0.7, 10, params.max_candidates, true); // @todo add to parameter list  

  CurveMatch::endCurve(id);
}

