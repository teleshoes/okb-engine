/* this file implement the curve matching algorithm
   for keyboard définition and curve (list of points) it produces a list of candidate ranked with score 
   the incremental / asynchronous algorithm is implemented in incr_match.{h,cpp} */

#ifndef CURVE_MATCH_H
#define CURVE_MATCH_H

#include <QList>
#include <QString>
#include <QHash>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonValue>
#include <QDebug>
#include <QTime>

#include "tree.h"

#define DBG(args...) { if(debug) { qDebug(args); } }

/* point 
   this is probably a bad case of NIH, but i had to learn to code in C++ :-) */
class Point {
 public:
  Point();
  Point(int x, int y);
  int x;
  int y;
  Point operator-(const Point &other);
  Point operator+(const Point &other);
  Point operator*(const float &other);
};

class CurvePoint : public Point {
 public:
  CurvePoint(Point p, int msec);
  int t;
  int speed;
  bool sharp_turn; // points with significant direction change -> they show user intention and must be correlated with a letter in a word
  int turn_angle;
  int turn_smooth;
  float normalx, normaly;
};

/* just a key */
class Key {
 public:
  Key(int x, int y, int width, int height, char label);
  Key();
  void toJson(QJsonObject &json) const;
  static Key fromJson(const QJsonObject &json);
  int x; // key center
  int y; 
  int width;
  int height;
  char label;
};

/* settings */
class Params {
 public:
  /* BEGIN PARAMS */
  float dist_max_start;
  float dist_max_next;
  int match_wait;
  int max_angle;
  int max_turn_error1;
  int max_turn_error2;
  int max_turn_error3;
  int length_score_scale;
  int curve_score_min_dist;
  float score_pow;
  float weight_distance;
  float weight_cos;
  float weight_length;
  float weight_curve;
  float weight_curve2;
  float weight_turn;
  float length_penalty;
  int turn_threshold;
  int max_turn_index_gap;
  int curve_dist_threshold;
  float small_segment_min_score;
  float anisotropy_ratio;
  float sharp_turn_penalty;
  int curv_amin;
  int curv_amax;
  int curv_turnmax;
  int max_active_scenarios;
  int max_candidates;
  float score_coef_speed;
  int angle_dist_range;
  int incremental_length_lag;
  /* END PARAMS */

  void toJson(QJsonObject &json) const;
  static Params fromJson(const QJsonObject &json);
};

/* tree traversal evaluation */
typedef struct {
  float distance_score;
  float cos_score;
  float curve_score;
  float length_score;
  float turn_score;
  float curve_score2;
} score_t;

/* scenario (describe word candidates) */
class Scenario {
 private:
  QList<QPair<LetterNode, int> > index_history;
  LetterNode node;
  bool finished;
  QString name;
  int count;
  int index;

  QList<score_t> scores;
  float temp_score;
  float final_score;

  QHash<unsigned char, Key> *keys;
  QList<CurvePoint> *curve;
  Params *params;
  QString logFile;
  int last_fork;

  bool debug;

 private:
  float calc_distance_score(unsigned char letter, int index, int count);
  float calc_cos_score(unsigned char prev_letter, unsigned char letter, int index, int new_index);
  float calc_curve_score(unsigned char prev_letter, unsigned char letter, int index, int new_index);
  float calc_length_score(unsigned char prev_letter, unsigned char letter, int index, int new_index);
  float calc_turn_score(unsigned char letter, int index);
  float calc_curviness_score(int index);
  float begin_end_angle_score(bool end);
  Point curve_tangent(int index);
  float get_next_key_match(unsigned char letter, int index, QList<int> &new_index, bool &overflow);
  float speedCoef(int index);
  float evalScore();

 public:
  Scenario(LetterTree *tree, QHash<unsigned char, Key> *keys, QList<CurvePoint> *curve, Params *params);
  void setDebug(bool debug);
  void nextKey(QList<Scenario> &result, int &st_fork);
  QList<LetterNode> getNextKeys();
  bool childScenario(LetterNode &child, bool endScenario, QList<Scenario> &result, int &st_fork, bool incremental = false);
  bool operator<(const Scenario &other) const;
  bool isFinished() { return finished; };
  QString getName() { return name; };
  QString getWordList();
  float getScore() const;
  void postProcess();
  float getTempScore() const;
  float getCount();
  bool forkLast();
  int getCurveIndex();

  void toJson(QJsonObject &json);
};

/* main processing for curve matching */
class CurveMatch {
 protected:
  QList<Scenario> scenarios;
  QList<Scenario> candidates;
  QList<CurvePoint> curve;
  QHash<unsigned char, Key> keys;
  Params params;
  LetterTree wordtree;
  bool loaded;
  int st_time, st_count, st_fork, st_skim;
  QString treeFile;
  QString logFile;
  QTime startTime;
  int id;
  bool debug;
  bool done;

  void scenarioFilter(QList<Scenario> &scenarios, float score_ratio, int min_size, int max_size = -1, bool finished = false);
  void curvePreprocess1(int last_curve_index = -1);
  void curvePreprocess2();

 public:
  CurveMatch();
  bool loadTree(QString file);
  void clearKeys();
  void addKey(Key key);
  virtual void clearCurve();
  virtual void addPoint(Point point, int timestamp = -1);
  virtual void endCurve(int id);
  bool match();
  QList<Scenario> getCandidates();
  QList<CurvePoint> getCurve();

  void setLogFile(QString fileName);
  void log(QString txt);

  void fromJson(const QJsonObject &json);
  void fromString(const QString &jsonStr);
  
  void toJson(QJsonObject &json);
  QString toString(bool indent = false);
  void resultToJson(QJsonObject &json);
  QString resultToString(bool indent = false);
  void setParameters(QString jsonStr);
  void useDefaultParameters();

  void setDebug(bool debug);
};

#endif /* CURVE_MATCH_H */
