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
#include <QSharedPointer>

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

#define FLAG_HINT_O (1<<0)
#define FLAG_HINT_V (1<<1)
#define FLAG_HINT_o (1<<2)
#define FLAG_HINT_ANY (FLAG_HINT_O | FLAG_HINT_V | FLAG_HINT_o)

#define FLAG_STR "OVo"

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
  int d2x, d2y, lac;
  int smoothx, smoothy;
  int flags;
};

class EndMarker : public CurvePoint {
 public:
  EndMarker(int curve_id) : CurvePoint(Point(), curve_id, 0) { this -> end_marker = true; };
};

/* just a key */
class Key {
 public:
  Key(int x, int y, int width, int height, QString label);
  Key();
  void toJson(QJsonObject &json) const;
  static Key fromJson(const QJsonObject &json);
  int x; // key center
  int y;
  int width;
  int height;
  QString label;
  int corrected_x;
  int corrected_y;
  unsigned char letter;
  unsigned char internal_letter;
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
  int *flags;
  Point *points;
  int count;

 public:
  QuickCurve(QList<CurvePoint> &curve, int curve_id = 0, int min_length = 1);
  QuickCurve();
  ~QuickCurve();
  void setCurve(QList<CurvePoint> &curve, int curve_id = 0, int min_length = 1);
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
  inline int getTimestamp(int index);
  /* inline is probably useless nowadays */

  int size();
  int getFlags(int index);
  bool hasFlags(int index, int mask);
  int getHintOIndex(int index0, bool incremental = false);

  bool finished;
  bool on_hold;
  bool isDot;
  float straight;

  int getCount() { return count; }
  int getTotalLength();
  int getLength(int index);
};

/* quick key information implementation */
#define QUICKKEYS_KEYS_PER_LETTER 5

class QuickKeys {
 private:
  Params *params;
  Point points_raw[256];
  Point points[256];
  Point dim[256];
  char m_quadrant[256];
  unsigned char letter2keys[QUICKKEYS_KEYS_PER_LETTER * 256];
 public:
  QuickKeys(QHash<QString, Key> &keys);
  QuickKeys();
  ~QuickKeys();
  void setKeys(QHash<QString, Key> &keys);
  inline Point const& get(unsigned char letter) const;
  inline Point const& size(unsigned char letter) const;
  inline Point const& get_raw(unsigned char letter) const;
  char const& quadrant(unsigned char letter) const;
  unsigned char* getKeysForLetter(unsigned char letter);
  unsigned char getLetterFromKey(unsigned char key);
  void setParams(Params *params) { this->params = params; };

  // statistics
  int average_width, average_height;
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
class ScenarioDto {
 private:
  QString name;
  QString wordList;
  float score;
  int cls;
  bool star;

 public:
  ScenarioDto(QString name, QString wordlist, float score, int cls, bool star) : name(name), wordList(wordlist), score(score), cls(cls), star(star) {}

  float getScore() { return score; }
  QString getName() { return name; }
  int getClass() { return cls; }
  bool getStar() { return star; }
  QString getWordList() { return wordList; }
};

#ifdef INCREMENTAL
class DelayedScenario;
#endif /* INCREMENTAL */

typedef struct {
  int st_time, st_count, st_fork, st_skim;
  int st_speed, st_special, st_retry;
  int st_cache_hit, st_cache_miss;
  int st_cputime;
} stats_t;

typedef struct {
  int direction; // was char, but char to int conversion seems to handle these as unsigned chars (didn't investigate)
  int corrected_direction;
  float expected;
  float expected_real;
  float actual;
  float corrected;
  float length_before;
  float length_after;
  int index;
  int start_index;
  bool unmatched;
  int length;
} turn_t;

typedef struct {
  float expected;
  int length_before;
  int length_after;
  // complete as needed
} simple_turn_t;

class NextIndex {
 public:
  int index;
  float score;
  NextIndex(int index, float score) { this->index = index; this->score = score; }
};

class MiscAcct {
 public:
  QString coef_name;
  float coef_value;
  float value;
  MiscAcct(QString &coef_name, float coef_value, float value) {
    this->coef_name = coef_name; this->coef_value = coef_value; this -> value = value;
  }
};

class Scenario;
typedef QHash<unsigned char, QList<Scenario> > child_cache_t;

#ifdef INCREMENTAL
class MultiScenario;
#endif /* INCREMENTAL */

class Scenario {
#ifdef INCREMENTAL
  friend class DelayedScenario;
  friend class MultiScenario;
#endif /* INCREMENTAL */


 protected:
  unsigned char *index_history; // let's not implement >255 letters words :-)
  unsigned char *letter_history;
  score_t *scores;

  LetterNode node;
  bool finished;
  int count;
  int index;

  float temp_score;

  float final_score;
  float score_v1;

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

  float new_dist;

  unsigned char fallback_count;

  // cache
  bool cache;
  QSharedPointer<child_cache_t> cacheChilds;
  // no significant impact: @todo try this with aggressive mode: QHash<unsigned char, int> cacheMinLength;

  QList<MiscAcct> *misc_acct;

  char quadrant;

 private:
  float calc_distance_score(unsigned char letter, int index, int count, float *return_distance = NULL);
  float calc_cos_score(unsigned char prev_letter, unsigned char letter, int index, int new_index);
  float calc_curve_score(unsigned char prev_letter, unsigned char letter, int index, int new_index);
  void calc_turn_score_all(turn_t *turn_detail, int *turn_count_return, simple_turn_t *turns);
  void check_turn2(simple_turn_t *turns, turn_t *turn_detail, int turn_count);
  int get_turn_kind(int index);
  void check_reverse_turn(int index1, int index2, int direction1, int direction2, bool check_hints = true);
  float calc_score_misc(int i);
  float begin_end_angle_score(bool end);
  float score_inflection(int index, bool st1, bool st2);
  Point computed_curve_tangent(int index);
  Point actual_curve_tangent(int i);
  float get_next_key_match(unsigned char letter, int index, QList<NextIndex> &new_index, bool incremental, bool &overflow);
  float evalScore();
  void copy_from(const Scenario &from);
  bool childScenarioInternal(LetterNode &child, QList<Scenario> &result, int &st_fork, bool incremental, bool endScenario);
  bool childScenarioInternalWithLetter(unsigned char letter, LetterNode &child, QList<Scenario> &result,
				       int &st_fork, bool incremental, bool endScenario);
  int getLocalTurn(int index);
  void turn_transfer(int turn_count, turn_t *turn_detail);
  void calc_straight_score_all(turn_t *turn_detail, int turn_count, float straight_score);
  void calc_loop_score_all(turn_t *turn_detail, int turn_count);
  void descent(LetterNode currentNode, QList<QPair<LetterNode, QString> > &result, unsigned char *pname);
  void calc_flat2_score_all();
  void calc_flat2_score_part(int i1, int i2);
  int calc_flat2_get_height(int i1, int i2);
  void log_misc(QString word, QString coef_name, float coef_value, float value);

 public:
  Scenario(LetterTree *tree, QuickKeys *keys, QuickCurve *curve, Params *params);
  Scenario(const Scenario &from);
  Scenario& operator=( const Scenario &from );
  ~Scenario();
  void setDebug(bool debug);
  void nextKey(QList<Scenario> &result, stats_t &st);
  QList<LetterNode> getNextKeys();
  bool childScenario(LetterNode &child, QList<Scenario> &result, stats_t &st, int curve_id = -1, bool incremental = false);
  bool childScenario(LetterNode &child, QList<Scenario> &result, stats_t &st, int curve_id, bool incremental, bool hasPayload, bool isLeaf);
  bool operator<(const Scenario &other) const;
  bool isFinished() { return finished; };
  QString getName() const;
  QString getNameRealLetters() const;
  unsigned char* getNameCharPtr() const;
  QString getWordList();
  QStringList getWordListAsList();
  float getScore() const;
  bool postProcess(stats_t &st);
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
  void setScore(float score) { final_score = score; }
  void toJson(QJsonObject &json);
  QString toString(bool indent = false);
  float getDistSqr() { return dist_sqr; }
  int getTimestamp() const;
  score_t getScoreIndex(int i);
  QString getId() const { return getName(); }
  bool nextLength(unsigned char next_letter, int curve_id, int &min, int &max);
  float getScoreV1() { return score_v1; };
  void setCache(bool value);

  void newDistance();
  float getNewDistance() { return new_dist; };

  static void sortCandidates(QList<Scenario *> candidates, Params &params, int debug);

  void setCurveCount(int) {}; // for compatibility only

  void deepDive(QList<Scenario> &result, float min_score = 0.);

  QList<QPair<unsigned char, Point> > get_key_error(void);

  static int checkHints(QString, unsigned char*, int*, int, bool debug_rules = false);

  char getQuadrant() { return quadrant; };
  void setQuadrant(char value) { quadrant = (value * quadrant < 0)?0:value; };
};


#endif /* SCENARIO_H */
