/* this is the incremental curve matching implementation,
   it works while you're drawing a curve */

#ifndef INCR_MATCH_H
#define INCR_MATCH_H

#include "config.h"

#ifdef INCREMENTAL

#ifndef MULTI
#error "INCREMENTAL requires MULTI"
#endif

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
  bool multi;
  int curve_count;
  Params *params;
  QSharedPointer<MultiScenario> multi_p;
  QSharedPointer<Scenario> single_p;

 public:

  bool dead;
  QHash<unsigned char, NextLetter> next;
  bool nextOk;

  DelayedScenario(LetterTree *tree, QuickKeys *keys, QuickCurve *curves, Params *params);
  DelayedScenario(const DelayedScenario &from);
  DelayedScenario(const MultiScenario &from);
  DelayedScenario(const Scenario &from);
  DelayedScenario& operator=(const DelayedScenario &from);
  ~DelayedScenario();

  bool isFinished();
  LetterNode getNode();
  bool nextLength(unsigned char next_letter, int curve_id, int &min_length, int &max_length);
  int getTotalLength(int curve_id);
  void setCurveCount(int count);
  QString getId();
  float getScore();
  QString getWordList();
  MultiScenario getMultiScenario();
  int getCount();
  int forkLast();
  QString getName();

  bool operator<(const DelayedScenario &other) const;
  void die() { dead = true; }

  void updateNextLength();
  void getChildsIncr(QList<DelayedScenario> &childs, bool finished, stats_t &st, bool recursive = true, float aggressive = 0);
  int getNextLength(int curve_id, float aggressive = 0);

  void setDebug(bool value);

  void display(char *prefix = NULL);

  void deepDive(QList<Scenario> &result);
};

class IncrementalMatch : public CurveMatch {
 protected:
  void incrementalMatchBegin();
  void incrementalMatchUpdate(bool finished, float aggressive = 0);
  QString getLengthStr();

  void update_next_iteration_length(float aggressive);

  void delayedScenariosFilter();

  void purge_snapshots();

  int next_iteration_index;
  int next_iteration_length[MAX_CURVES];
  int current_length[MAX_CURVES];
  int last_curve_count;

  QElapsedTimer timer;
  double start_cpu_time;

  QList<DelayedScenario> *delayed_scenarios_p;

  int last_snapshot_count;
  QList< QList<DelayedScenario> *> ds_snapshots;
  void fallback(QList<ScenarioType> &result);

 public:
  IncrementalMatch();
  virtual ~IncrementalMatch();
  virtual void addPoint(Point point, int curve_id, int timestamp = -1);
  virtual void endOneCurve(int curve_id);
  virtual void endCurve(int id);
  void aggressiveMatch();
};

#endif /* INCREMENTAL */

#endif /* INCR_MATCH_H */
