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
#include "log.h"

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
  CurvePoint(Point p, int msec);
  int t;
  int speed;
  int sharp_turn; // points with significant direction change -> they show user intention and must be correlated with a letter in a word
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
  Point *points;
  int count;
 public:
  QuickCurve(QList<CurvePoint> &curve);
  QuickCurve();
  ~QuickCurve();
  void setCurve(QList<CurvePoint> &curve);
  void clearCurve();
  inline Point const& point(int index) const;
  inline int getX(int index);
  inline int getY(int index);
  inline int getTurn(int index);
  inline int getTurnSmooth(int index);
  inline int getSharpTurn(int index);
  inline int getNormalX(int index);
  inline int getNormalY(int index);
  inline int getSpeed(int index);
  inline Point getNormal(int index);
  inline int size();
  /* inline is probably useless nowadays */
};

/* quick key information implementation */
class QuickKeys {
 private:
  Point* points;
 public:
  QuickKeys(QHash<unsigned char, Key> &keys);
  QuickKeys();
  ~QuickKeys();
  void setKeys(QHash<unsigned char, Key> &keys);
  inline Point const& get(unsigned char letter) const;
};

/* settings */
class Params {
 public:
  /* BEGIN PARAMS */
  float dummy;
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
  int turn_threshold2;
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
  int incremental_index_gap;
  float crv2_weight;
  float crv_st_bonus;
  int crv_concavity_amin;
  int crv_concavity_amax;
  int crv_concavity_max_turn;
  float same_point_score;
  float curve_surface_coef;
  float coef_length;
  int tip_amin;
  int tip_amax;
  int crv_st_amin;
  int inflection_min_angle;
  float inflection_coef;
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
  unsigned char *index_history; // let's not implement >255 letters words :-)
  unsigned char *letter_history;
  score_t *scores;

  LetterNode node;
  bool finished;
  int count;
  int index;

  float temp_score;
  float final_score;

  int last_fork;

  bool debug;

  QuickKeys *keys;
  QuickCurve *curve;
  Params *params;

 private:
  float calc_distance_score(unsigned char letter, int index, int count);
  float calc_cos_score(unsigned char prev_letter, unsigned char letter, int index, int new_index);
  float calc_curve_score(unsigned char prev_letter, unsigned char letter, int index, int new_index);
  float calc_length_score(unsigned char prev_letter, unsigned char letter, int index, int new_index);
  float calc_turn_score(unsigned char letter, int index);
  float calc_curviness_score(int index);
  float calc_curviness2_score(int index);
  float begin_end_angle_score(bool end);
  float score_inflection(int index, bool st1, bool st2);
  Point computed_curve_tangent(int index);
  float get_next_key_match(unsigned char letter, int index, QList<int> &new_index, bool &overflow);
  float lengthCoef(float length);
  float speedCoef(int index);
  float evalScore();
  void copy_from(const Scenario &from);

 public:
  Scenario(LetterTree *tree, QuickKeys *keys, QuickCurve *curve, Params *params);
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
  unsigned char* getNameCharPtr();
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
