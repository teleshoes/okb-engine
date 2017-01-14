#include <iostream>

#include "curve_plugin.h"

using namespace std;

#ifdef THREAD
#define WF_IDLE { thread.waitForIdle(); }
#else
#define WF_IDLE
#endif /* THREAD */

/* thread callback handler */
#ifdef THREAD
PluginCallBack::PluginCallBack(CurveKB *p) : plugin(p) {}

void PluginCallBack::call(QList<ScenarioDto> l) {
  plugin->sendSignal(l);
}
#endif /* THREAD */

/* registered class */
CurveKB::CurveKB(QObject *parent) : QObject(parent)
{
#ifdef THREAD
  callback = new PluginCallBack(this);
  thread.setCallBack(callback);
  thread.setMatcher(&curveMatch);
#endif /* THREAD */
}

CurveKB::~CurveKB()
{
#ifdef THREAD
  thread.stopThread();
  delete callback;
#endif /* THREAD */
}

void CurveKB::learn(QString word, int addValue)
{
#ifdef THREAD
  thread.learn(word, addValue);
#else
  curveMatch.learn(word, addValue);
#endif /* THREAD */
}

void CurveKB::startCurve()
{
#ifdef THREAD
  thread.clearCurve();
#else
  curveMatch.clearCurve();
#endif /* THREAD */
}

void CurveKB::addPoint(int x, int y, int curve_id)
{
#ifdef THREAD
  thread.addPoint(Point(x,y), curve_id);
#else
  curveMatch.addPoint(Point(x,y), curve_id);
#endif /* THREAD */
}

void CurveKB::endOneCurve(int curve_id) {
#ifdef THREAD
  thread.endOneCurve(curve_id);
#else
  curveMatch.endOneCurve(curve_id);
#endif /* THREAD */
}

void CurveKB::endCurve(int correlation_id)
{
#ifdef THREAD
  thread.endCurve(correlation_id);
  WF_IDLE; // synchronous call, so we have to wait until completion
#else
  curveMatch.endCurve(correlation_id);
#endif /* THREAD */
}

void CurveKB::endCurveAsync(int correlation_id)
{
#ifdef THREAD
  thread.endCurve(correlation_id);
#else
  curveMatch.endCurve(correlation_id); // there will be no callback
#endif /* THREAD */
}

void CurveKB::resetCurve()
{
#ifdef THREAD
  thread.clearCurve();
#else
  curveMatch.clearCurve();
#endif /* THREAD */
}

void CurveKB::sendSignal(QList<ScenarioDto> &candidates)
{
  emit matchingDone(scenarioList2QVariantList(candidates));
}

QVariantList CurveKB::scenarioList2QVariantList(QList<ScenarioDto> &candidates) {
  QVariantList ret;
  foreach (ScenarioDto scenario, candidates) {
    QVariantList item;
    item << scenario.getName() << scenario.getScore() << scenario.getClass() << scenario.getStar() << scenario.getWordList();
    ret.append((QVariant) item); // avoid list flattening
  }
  return ret;
}

void CurveKB::loadKeys(QVariantList list)
{
  WF_IDLE;
  curveMatch.clearKeys();
  QListIterator<QVariant> i(list);
  while (i.hasNext()) {
    QMap<QString, QVariant> key = i.next().toMap();
    QByteArray bytes = key["caption"].toByteArray();

    int x = key["x"].toInt();
    int y = key["y"].toInt();
    int width = key["width"].toInt();
    int height = key["height"].toInt();
    QString caption = key["caption"].toString();

    curveMatch.addKey(Key(int(x + width / 2), int(y + height / 2), width, height, caption));
  }
}

bool CurveKB::loadTree(QString fileName)
{
  // loading will be done in separate thread, but at list check now il file exists
  QFile file(fileName);
  if (! file.exists()) {
    return false;
  }

#ifdef THREAD
  thread.loadTree(fileName);
  return true;
#else
  return curveMatch.loadTree(fileName);
#endif /* THREAD */
}

void CurveKB::setLogFile(QString fileName)
{
  WF_IDLE;
  curveMatch.setLogFile(fileName);
}

QString CurveKB::getResultJson()
{
  WF_IDLE;
  return curveMatch.toString(false);
}

void CurveKB::setDebug(bool debug)
{
  WF_IDLE;
  curveMatch.setDebug(debug);
}

void CurveKB::loadParameters(QString params)
{
  WF_IDLE;
  curveMatch.setParameters(params);
}

QVariantList CurveKB::getCandidates()
{
  WF_IDLE;
  QList<ScenarioDto> candidates = curveMatch.getCandidatesDto();
  return scenarioList2QVariantList(candidates);
}

/* plugin */
CurveExtensionPlugin::CurveExtensionPlugin()
{
}

CurveExtensionPlugin::~CurveExtensionPlugin()
{
}

void CurveExtensionPlugin::initializeEngine(QQmlEngine *, const char *uri)
{
    Q_ASSERT(QString(CURVE_PLUGIN_ID) == uri);

    // init here :-)
}

void CurveExtensionPlugin::registerTypes(const char *uri)
{
    Q_ASSERT(QString(CURVE_PLUGIN_ID) == uri);

    qmlRegisterType<CurveKB>(uri, 1, 0, CURVE_CLASS_NAME);
}

double CurveKB::getScalingRatio() {
  return (double) curveMatch.getScalingRatio();
}
