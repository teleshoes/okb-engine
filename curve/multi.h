/* This class implement multi-touch swiping
   (whatever that could mean) */

#ifndef MULTI_H
#define MULTI_H

#include "config.h"

#ifdef MULTI

#include <QString>
#include <QList>
#include <QSharedPointer>

#include "log.h"
#include "scenario.h"

#define MAX_CURVES 20

typedef struct {
  char curve_id;
  char index;
  int curve_index;
  bool end_curve;
} history_t;

#ifdef INCREMENTAL
class DelayedScenario;
#endif /* INCREMENTAL */

/* a "MultiScenario" is a scenario that aggregate multiple scenarios (one for
   each curve drawn in a multi-touch context) */
class MultiScenario {
#ifdef INCREMENTAL
  friend class DelayedScenario;
#endif /* INCREMENTAL */

 protected:
  QuickKeys *keys;
  QuickCurve *curves;
  Params *params;

  // MultiScenario instances can have child which share basic scenarios with
  // their parent. To avoid unnecessary copying use QT smart pointers
  QList<QSharedPointer<Scenario> > scenarios;

  LetterNode node;

  bool finished, debug, zombie;
  float dist, dist_sqr;

  int count;
  int curve_count;

  int ts;
  int id;

  float final_score;

  history_t *history;
  unsigned char *letter_history;

  void copy_from(const MultiScenario &from);

  void addSubScenarios();

  static int global_id; /* yuck global variable to track current global id */
  static QList<Scenario> scenario_root;

 public:
  MultiScenario(LetterTree *tree, QuickKeys *keys, QuickCurve *curves, Params *params);
  MultiScenario(const MultiScenario &from);
  MultiScenario(const Scenario &from);
  MultiScenario& operator=( const MultiScenario &from );
  ~MultiScenario();

  bool childScenario(LetterNode &child, QList<MultiScenario> &result, stats_t &st, int curve_id = -1, bool incremental = false);
  void nextKey(QList<MultiScenario> &result, stats_t &st);
  QList<LetterNode> getNextKeys();

  QString getName() const;
  void setCurveCount(int count) { this -> curve_count = count; }
  void setDebug(bool debug) { this -> debug = debug; }
  bool operator<(const MultiScenario &other) const;
  bool isFinished() { return finished; };
  QString getName();
  unsigned char* getNameCharPtr() const;
  QString getWordList();
  QStringList getWordListAsList();
  float getScore() const;
  bool postProcess(stats_t &st);
  float getCount() const;
  bool forkLast();
  float getTempScore() const;
  int getErrorCount();
  int getGoodCount();
  float distance() const;
  void toJson(QJsonObject &json);
  QString toString(bool indent = false);
  void PostProcess();
  void setScore(float score) { this -> final_score = score; }
  score_t getScores();
  QString getId() const;
  bool nextLength(unsigned char next_letter, int curve_id, int &min, int &max);
  float getScoreV1() const;

  static void sortCandidates(QList<MultiScenario *> candidates, Params &params, int debug);
  static void init();

  float getNewDistance();

  /* deprecated */
  int getClass() { return 0; }
  int getStar() { return 0; }
};

#endif /* MULTI */

#endif /* MULTI_H */

