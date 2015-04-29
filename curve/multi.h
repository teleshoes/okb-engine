/* This class implement multi-touch swiping 
   (whatever that could mean) */

#ifndef MULTI_H
#define MULTI_H

#define MAX_CURVES 20

#include <QString>
#include <QList>
#include <QSharedPointer>

#include "scenario.h"

class NextLetter {
 public:
  LetterNode letter_node;
  int next_length_min;
  int next_length_max;
  NextLetter(LetterNode, int, int);
  NextLetter();
};

typedef struct {
  char curve_id;
  char index;
  int curve_index;
  bool end_curve;
} history_t;

/* a "MultiScenario" is a scenario that aggregate multiple scenarios (one for
   each curve drawn in a multi-touch context) */
class MultiScenario {
 private:
  QuickKeys *keys;
  QuickCurve **curves;
  Params *params;

  // MultiScenario instances can have child which share basic scenarios with 
  // their parent. To avoid unnecessary copying use QT smart pointers
  QList<QSharedPointer<Scenario> > scenarios;
  
  LetterNode node;

  bool dead, finished, debug;
  float dist, dist_sqr;

  int count;
  int curve_count;

  int ts;

  float final_score, final_score2, final_score_v1, temp_score;

  history_t *history;
  unsigned char *letter_history;

  void copy_from(const MultiScenario &from);

 public:
  MultiScenario(LetterTree *tree, QuickKeys *keys, QuickCurve **curves, Params *params);
  MultiScenario(const MultiScenario &from);
  MultiScenario& operator=( const MultiScenario &from );
  ~MultiScenario();
    
  bool childScenario(LetterNode &child, bool endScenario, QList<MultiScenario> &result, int &st_fork, bool incremental = false);
  void nextKey(QList<MultiScenario> &result, int &st_fork);
  QList<LetterNode> getNextKeys();

  // @todo QHash<unsigned char, NextLetter> getNextLettersWaitInfo();

  QString getId() const;
  void setCurveCount(int count) { this -> curve_count = count; }
  void setDebug(bool debug) { this -> debug = debug; }
  bool operator<(const MultiScenario &other) const;
  bool isFinished() { return finished; };
  QString getName();
  unsigned char* getNameCharPtr() const;
  QString getWordList();
  float getScore() const;
  bool postProcess();
  float getCount() const;
  bool forkLast();
  void getDetailedScores(score_t &avg, score_t &min) const;
  float getTempScore() const;
  int getErrorCount();
  void evalScore(float &score, float &min_score, score_t &avg, score_t &min) const;
  float getMinScore();
  score_t getMinScores();
  score_t getScores();
  int getGoodCount();
  float distance() const;
  void setScore(float score) { final_score2 = score; }
  float getScoreOrig() const { return final_score; }
  void setScoreV1(float score) { final_score_v1 = score; }
  void toJson(QJsonObject &json);
  QString toString(bool indent = false);

  /* deprecated */
  int getClass() { return 0; }
  int getStar() { return 0; }
};

class DelayedScenario : public MultiScenario {
  // @todo
};


#endif /* MULTI_H */

