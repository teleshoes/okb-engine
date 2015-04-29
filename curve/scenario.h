/* this file implement the curve matching algorithm
   for keyboard definition and curve (list of points) it produces a list of candidate ranked with score 
   the incremental / asynchronous algorithm is implemented in incr_match.{h,cpp} */

#ifndef SCENARIO_H
#define SCENARIO_H

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
#include "log.h"

#include "params.h"

#define DBG(args...) { if (debug) { logdebug(args); } }

/* point 
   this is probably a bad case of NIH :-) */
class Point {
 public:
  Point();
  Point(int x, int y);
  int x;
  int y;
  Point const operator-(const Point &other) const;
  Point const operator+(const Point &other) const;
  Point const operator*(const float &other) const;
};

class CurvePoint : public Point {
 public:
  CurvePoint(Point p, int curve_id, int msec, int length = 0, bool dummy = false);
  void toJson(QJsonObject &json) const;
  bool operator<(const CurvePoint &other) const;
  static CurvePoint fromJson(const QJsonObject &json);
  int t;
  int curve_id; // for multi-touch swipes
  int speed;
  int sharp_turn; // points with significant direction change -> they show user intention and must be correlated with a letter in a word
  int turn_angle;
  int turn_smooth;
  float normalx, normaly;
  int length;
  bool dummy;
  bool end_marker;
};

class EndMarker : public CurvePoint {
 public:
  EndMarker(int curve_id) : CurvePoint(Point(), curve_id, 0) {};
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
  int corrected_x;
  int corrected_y;
};

/* quick curve implementation */
class QuickCurve {
  // accessing QList<CurvePoint> continually proved to be too slow according to profiler
  // so this is a naive C-like implementation
 private:
  int *x;
  int *y;
  int *turn;
  int *turnsmooth;
  int *sharpturn;
  int *normalx;
  int *normaly;
  int *speed;
  int *length;
  int *timestamp;
  int *ts;
  Point *points;

  int count;
 public:
  QuickCurve(QList<CurvePoint> &curve, int curve_id = 0);
  QuickCurve();
  ~QuickCurve();
  void setCurve(QList<CurvePoint> &curve, int curve_id = 0);
  void clearCurve();
  inline Point const& point(int index) const;
  inline int getX(int index);
  inline int getY(int index);
  inline int getTurn(int index);
  inline int getTurnSmooth(int index);
  inline int getSharpTurn(int index);
  inline int getSpecialPoint(int index);
  inline Point getNormal(int index);
  inline int getNormalX(int index);
  inline int getNormalY(int index);
  inline int getSpeed(int index);
  inline int getCurveId(int index);
  inline int getLength(int index);
  inline int size();
  inline int getTimestamp(int index);
  /* inline is probably useless nowadays */

  bool finished;
};

/* quick key information implementation */
class QuickKeys {
 private:
  Point* points;
  Point* dim;
 public:
  QuickKeys(QHash<unsigned char, Key> &keys);
  QuickKeys();
  ~QuickKeys();
  void setKeys(QHash<unsigned char, Key> &keys);
  inline Point const& get(unsigned char letter) const;
  inline Point const& size(unsigned char letter) const;
};

/* tree traversal evaluation */
typedef struct {
  float distance_score;
  float turn_score;
  float cos_score;
  float curve_score;
  float misc_score;
  float length_score;
} score_t;

#define SCORE_T_OFFSET (sizeof(float))
#define SCORE_T_COUNT ((int) (sizeof(score_t) / SCORE_T_OFFSET))
#define SCORE_T_GET(score, i) (((float*) &(score))[i])


/* scenario (describe word candidates) */
class Scenario {
 private:
  unsigned char *index_history; // let's not implement >255 letters words :-)
  unsigned char *letter_history;
  score_t *scores;

  LetterNode node;
  bool finished;
  int count;
  int index;

  float temp_score;
  float final_score;
  float final_score2;
  float final_score_v1;

  float dist;
  float dist_sqr;

  int last_fork;

  bool debug;

  QuickKeys *keys;
  QuickCurve *curve;
  Params *params;

  score_t avg_score;
  score_t min_score;

  int result_class;
  bool star;

  int error_count;

  float min_total;

  int good_count;

 private:
  float calc_distance_score(unsigned char letter, int index, int count, float *return_distance = NULL);
  float calc_cos_score(unsigned char prev_letter, unsigned char letter, int index, int new_index);
  float calc_curve_score(unsigned char prev_letter, unsigned char letter, int index, int new_index);
  void calc_turn_score_all();
  int get_turn_kind(int index);
  void check_reverse_turn(int index1, int index2, int direction1, int direction2);
  float calc_score_misc(int i);
  float begin_end_angle_score(bool end);
  float score_inflection(int index, bool st1, bool st2);
  Point computed_curve_tangent(int index);
  Point actual_curve_tangent(int i);
  float get_next_key_match(unsigned char letter, int index, QList<int> &new_index, bool incremental, bool &overflow);
  float evalScore();
  void copy_from(const Scenario &from);

 public:
  Scenario(LetterTree *tree, QuickKeys *keys, QuickCurve *curve, Params *params);
  Scenario(LetterTree *tree, QuickKeys *keys, QuickCurve **curves, Params *params);
  Scenario(const Scenario &from);
  Scenario& operator=( const Scenario &from );
  ~Scenario();
  void setDebug(bool debug);
  void nextKey(QList<Scenario> &result, int &st_fork);
  QList<LetterNode> getNextKeys();
  bool childScenario(LetterNode &child, bool endScenario, QList<Scenario> &result, int &st_fork, bool incremental = false);
  bool operator<(const Scenario &other) const;
  bool isFinished() { return finished; };
  QString getName();
  unsigned char* getNameCharPtr() const;
  QString getWordList();
  float getScore() const;
  bool postProcess();
  float getTempScore() const;
  float getCount() const;
  bool forkLast();
  int getCurveIndex();
  void getDetailedScores(score_t &avg, score_t &min) const;
  void setClass(int cls) { result_class = cls; };
  int getClass() { return result_class; };
  void setStar(bool value) { star = value; };
  int getStar() { return star; }
  float getMinScore() { return min_total; }
  int getErrorCount() { return error_count; }
  score_t getScores() { return avg_score; }
  score_t getMinScores() { return min_score; }
  int getGoodCount() { return good_count; }
  float distance() const;
  void setScore(float score) { final_score2 = score; }
  float getScoreOrig() const { return final_score; }
  void setScoreV1(float score) { final_score_v1 = score; }
  void toJson(QJsonObject &json);
  QString toString(bool indent = false);
  float getDistSqr() { return dist_sqr; }
  int getTimestamp();
  score_t getScoreIndex(int i);
};

typedef struct {
  int st_time, st_count, st_fork, st_skim;
  int st_speed, st_special, st_retry;
} stats_t;

#endif /* SCENARIO_H */
