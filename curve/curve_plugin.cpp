#include <iostream>

#include "curve_plugin.h"

using namespace std;

#ifdef THREAD
#define WF_IDLE { thread.waitForIdle(); }
#else
#define WF_IDLE
#endif /* THREAD */

/* thread callback handler */
PluginCallBack::PluginCallBack(CurveKB *p) : plugin(p) {}

void PluginCallBack::call(QList<Scenario> l) {
  plugin->sendSignal(l);
}


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

void CurveKB::startCurve(int x, int y)
{
#ifdef THREAD
  thread.clearCurve();
  thread.addPoint(Point(x,y));
#else
  curveMatch.clearCurve();
  curveMatch.addPoint(Point(x,y));
#endif /* THREAD */
}

void CurveKB::addPoint(int x, int y) 
{
#ifdef THREAD
  thread.addPoint(Point(x,y));
#else
  curveMatch.addPoint(Point(x,y));
#endif /* THREAD */
}

void CurveKB::endCurve(int id)
{
#ifdef THREAD
  thread.endCurve(id);
  WF_IDLE; // synchronous call, so we have to wait until completion
#else
  curveMatch.endCurve(id);
#endif /* THREAD */
}
 
void CurveKB::endCurveAsync(int id)
{
#ifdef THREAD
  thread.endCurve(id);
#else
  curveMatch.endCurve(id);
  callBack(curveMatch.getCandidates());
#endif /* THREAD */
}
 
void CurveKB::sendSignal(QList<Scenario> &candidates) 
{
  emit matchingDone(scenarioList2QVariantList(candidates));
}

QVariantList CurveKB::scenarioList2QVariantList(QList<Scenario> &candidates) {
  QVariantList ret;
  foreach (Scenario scenario, candidates) {
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
    QString caption = key["caption"].toString();
    int x = key["x"].toInt();
    int y = key["y"].toInt();
    int width = key["width"].toInt();
    int height = key["height"].toInt();

    char c = caption.at(0).cell();
    curveMatch.addKey(Key(int(x + width / 2), int(y + height / 2), width, height, c));
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
  QList<Scenario> candidates = curveMatch.getCandidates();
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

