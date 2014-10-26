/* this is the incremental curve matching implementation,
   it works while you're drawing a curve */

#ifndef INCR_MATCH_H
#define INCR_MATCH_H

#include "curve_match.h"
#include "tree.h"

#include <QElapsedTimer>

/* A delayed scenario is a scenario with a minimal length for each possible child scenario
   it will be triggered when the actual curve lenght is greater than the miniman length
 */
class NextLetter {
 public:
  LetterNode letter_node;
  int next_length_min;
  int next_length_max;
  NextLetter(LetterNode, int, int);
  NextLetter();
};

class DelayedScenario {
 private:
  bool dead;

 public:
  Scenario scenario;
  QHash<unsigned char, NextLetter> next;

  DelayedScenario(Scenario &s, QHash<unsigned char, NextLetter> &h);
  bool operator<(const DelayedScenario &other) const;
  bool isDead() { return dead; }
  void die() { dead = true; }
};


class IncrementalMatch : public CurveMatch {
 protected:
  void incrementalMatchBegin();
  void incrementalMatchUpdate(bool finished, bool aggressive = false);

  void incrementalNextKeys(Scenario &scenario, QList<DelayedScenario> &result, bool finished);
  bool evalChildScenario(Scenario &scenario, LetterNode &childNode, QList<DelayedScenario> &result, bool finished);

  void update_next_iteration_length(int length);

  void displayDelayedScenario(DelayedScenario &ds);
  void delayedScenariosFilter();

  QList<DelayedScenario> delayed_scenarios; 
  int cumulative_length;
  int next_iteration_length;
  int last_curve_index;
  int next_iteration_index;

  QElapsedTimer timer;

  QuickCurve quickCurve;
  QuickKeys quickKeys;

 public:
  virtual void clearCurve();
  virtual void addPoint(Point point, int timestamp = -1);
  virtual void endCurve(int id);
  void aggressiveMatch();
};

#endif /* INCR_MATCH_H */
