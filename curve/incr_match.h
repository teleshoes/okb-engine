/* this is the incremental curve matching implementation,
   it works while you're drawing a curve */

#ifndef INCR_MATCH_H
#define INCR_MATCH_H

#include "config.h"

#ifdef INCREMENTAL

#include "curve_match.h"
#include "tree.h"

#include <QElapsedTimer>

/* A delayed scenario is a scenario which childs can not be evaluated right now because
   the user has only drawn a small part of the gesture (mono or multi-touch), so we
   have to wait until he draws more before evaluating child scenarios 
   -> this used for incremental mode */

class NextLetter {
 public:
  LetterNode letter_node;
  QList<int> next_length;
  NextLetter(LetterNode);
  NextLetter();
};

class DelayedScenario {
 private:
  void updateNextLetters();
  bool debug;

 public:
  ScenarioType scenario;
  bool dead;
  QHash<unsigned char, NextLetter> next;
  bool nextOk;

  DelayedScenario(LetterTree *tree, QuickKeys *keys, QuickCurve *curves, Params *params);
  DelayedScenario(const DelayedScenario &from);
  DelayedScenario(const ScenarioType &from);
  DelayedScenario& operator=(const DelayedScenario &from);
  ~DelayedScenario();

  bool operator<(const DelayedScenario &other) const;
  void die() { dead = true; }

  void updateNextLength();
  void getChildsIncr(QList<DelayedScenario> &childs, bool finished, stats_t &st, bool recursive = true, bool aggressive = false);
  int getNextLength(int curve_id, bool aggressive = false);

  void setDebug(bool value) { debug = scenario.debug = value; }

  void display(char *prefix = NULL);
};

class IncrementalMatch : public CurveMatch {
 protected:
  void incrementalMatchBegin();
  void incrementalMatchUpdate(bool finished, bool aggressive = false);
  QString getLengthStr();

  void update_next_iteration_length(bool aggressive);

  void delayedScenariosFilter();

  int next_iteration_index;
  int next_iteration_length[MAX_CURVES];
  int current_length[MAX_CURVES];
  int last_curve_count;

  QElapsedTimer timer;

  QuickKeys quickKeys;

  QList<DelayedScenario> delayed_scenarios;

 public:
  virtual void addPoint(Point point, int curve_id, int timestamp = -1);
  virtual void endOneCurve(int curve_id);
  virtual void endCurve(int id);
  void aggressiveMatch();
};

#endif /* INCREMENTAL */

#endif /* INCR_MATCH_H */
