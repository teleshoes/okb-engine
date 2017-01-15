/* this file implement the curve matching algorithm
   for keyboard definition and curve (list of points) it produces a list of candidate ranked with score
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

#include "config.h"
#include "tree.h"
#include "log.h"
#include "params.h"
#include "scenario.h"
#include "multi.h"
#include "key_shift.h"

#ifdef MULTI
typedef MultiScenario ScenarioType;
#else
typedef Scenario ScenarioType;
#endif /* MULTI */

double getCPUTime();

/* user dictionary entry */
class UserDictEntry {
 public:
  UserDictEntry() : letters(QString()), ts(0), count(0.0) {};
  UserDictEntry(QString _l, int _t, float _c) : letters(_l), ts(_t), count(_c) { };

  float getUpdatedCount(int now, int days) const;

  QString letters;
  int ts;
  float count;
};

/* main processing for curve matching */
class CurveMatch {
 protected:
  QList<ScenarioType> scenarios;
  QList<ScenarioType> candidates;
  QList<CurvePoint> curve;
  QHash<QString, Key> keys;
  Params params;
  LetterTree wordtree;
  bool userdict_dirty;
  bool loaded;
  QString treeFile;
  QString userDictFile;
  QString logFile;
  QTime startTime;
  int id;
  bool debug;
  bool done;
  int curve_length;
  int curve_count;

  bool curve_started[MAX_CURVES];

  stats_t st;

  bool kb_preprocess;

  bool straight;

  QHash<QString, UserDictEntry> userDictionary;

  void scenarioFilter(QList<ScenarioType> &scenarios, float score_ratio, int min_size, int max_size = -1, bool finished = false);
  bool curvePreprocess1(int curve_id = 0);
  void curvePreprocess2();

  int compare_scenario(ScenarioType *s1, ScenarioType *s2, bool reverse = false);

  QuickCurve quickCurves[MAX_CURVES + 1];

  bool setCurves();

  KeyShift keyShift;

  QuickKeys quickKeys;

  float scaling_ratio;
  int dpi;

  void computeScalingRatio();

 public:
  CurveMatch();
  virtual ~CurveMatch() {};
  bool loadTree(QString file);
  void clearKeys();
  void addKey(Key key);
  virtual void clearCurve();
  virtual void addPoint(Point point, int curve_id, int timestamp = -1);
  virtual void endOneCurve(int curve_id);
  virtual void endCurve(int correlation_id);
  bool match();
  QList<ScenarioType> getCandidates();
  QList<ScenarioDto> getCandidatesDto();
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
  Params* getParamsPtr();
  void useDefaultParameters();

  void setDebug(bool debug);

  void learn(QString word, int addValue = 1, bool init = false);
  void loadUserDict();
  void saveUserDict();
  void purgeUserDict();
  void dumpDict();
  char* getPayload(unsigned char *letters);

  void sortCandidates();

  void loadKeyPos();
  void saveKeyPos();
  void updateKeyPosForTest(QString expected);
  void storeKeyPos();

  float getScalingRatio() { computeScalingRatio(); return scaling_ratio; }
  void setDpi(int dpi);
};

#endif /* CURVE_MATCH_H */
