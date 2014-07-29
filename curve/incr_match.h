#ifndef INCR_MATCH_H
#define INCR_MATCH_H

#include "curve_match.h"
#include "tree.h"

/* A delayed scenario is a scenario with for each possible child scenario a letter note and a minimal length */
class DelayedScenario {
 public:
  Scenario scenario;
  QHash<unsigned char, QPair<LetterNode, int> > next;

  DelayedScenario(Scenario &s, QHash<unsigned char, QPair<LetterNode, int> > &h);
  bool operator<(const DelayedScenario &other) const;
};

class IncrementalMatch : public CurveMatch {
 protected:
  void incrementalMatchBegin();
  void incrementalMatchUpdate(bool finished);

  void incrementalNextKeys(Scenario &scenario, QList<DelayedScenario> &result, bool finished);
  void evalChildScenario(Scenario &scenario, LetterNode &childNode, QList<DelayedScenario> &result, bool finished);

  void update_next_iteration_length(int length);

  void displayDelayedScenario(DelayedScenario &ds);
  void delayedScenariosFilter();

  QList<DelayedScenario> delayed_scenarios; 
  int cumulative_length;
  QList<int> index2distance;
  int next_iteration_length;
  int last_curve_index;
  int next_iteration_index;

 public:
  virtual void clearCurve();
  virtual void addPoint(Point point);
  virtual void endCurve(int id);
};

#endif /* INCR_MATCH_H */
