#include <iostream>

#include "curve_plugin.h"

using namespace std;

/* registered class */
CurveKB::CurveKB(QObject *parent) : QObject(parent)
{
}

CurveKB::~CurveKB()
{
}

void CurveKB::startCurve(int x, int y)
{
  curveMatch.clearCurve();
  curveMatch.addPoint(Point(x,y));
}

void CurveKB::addPoint(int x, int y) 
{
  curveMatch.addPoint(Point(x,y));
}

void CurveKB::endCurve(int id)
{
  curveMatch.endCurve(id);
}

void CurveKB::loadKeys(QVariantList list) 
{
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
  return curveMatch.loadTree(fileName);
}

void CurveKB::setLogFile(QString fileName)
{
  curveMatch.setLogFile(fileName);
}

QString CurveKB::getResultJson()
{
  return curveMatch.toString(false);
}

void CurveKB::setDebug(bool debug)
{
  curveMatch.setDebug(debug);
}

void CurveKB::loadParameters(QString params)
{
  curveMatch.setParameters(params);
}

QVariantList CurveKB::getCandidates()
{
  QVariantList ret;
  foreach (Scenario scenario, curveMatch.getCandidates()) {
    QVariantList item;
    item << scenario.getName() << scenario.getScore() << scenario.getWordList();
    ret.append((QVariant) item); // avoid list flattening
  }
  return ret;
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

