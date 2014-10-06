#ifndef CURVE_PLUGIN_H
#define CURVE_PLUGIN_H

#include <QtQml>
#include <QQmlExtensionPlugin>

#include "curve_match.h"

#define THREAD 1

#ifdef THREAD
#include "thread.h"
#endif /* THREAD */

#define CURVE_PLUGIN_ID "eu.cpbm.okboard"
#define CURVE_CLASS_NAME "CurveKB"

#ifdef THREAD
class CurveKB; // forward decl

class PluginCallBack : public ThreadCallBack {
 private:
  CurveKB *plugin;
 public:
  PluginCallBack(CurveKB *plugin);
  void call(QList<Scenario>);
};
#endif /* THREAD */

/* registered classes */
class CurveKB : public QObject {
    Q_OBJECT

 private:
#ifdef THREAD
    CurveThread thread;
    IncrementalMatch curveMatch;
    PluginCallBack *callback;
#else
    CurveMatch curveMatch;    
#endif /* THREAD */

 public:
    CurveKB(QObject *parent = 0);
    virtual ~CurveKB();

    Q_INVOKABLE void startCurve(int x, int y);
    Q_INVOKABLE void addPoint(int x, int y);
    Q_INVOKABLE void endCurve(int id);
    Q_INVOKABLE void endCurveAsync(int id);
    Q_INVOKABLE void resetCurve();

    Q_INVOKABLE void loadKeys(QVariantList list);
    Q_INVOKABLE bool loadTree(QString fileName);
    Q_INVOKABLE void setLogFile(QString fileName);
    Q_INVOKABLE QString getResultJson();
    Q_INVOKABLE void setDebug(bool debug);
    Q_INVOKABLE void loadParameters(QString params);
      
    // i've never managed to return an object list from c++ to qml :-)
    Q_INVOKABLE QVariantList getCandidates();

    void sendSignal(QList<Scenario> &candidates);

 private:
    QObject m_keyboard;
    QVariantList scenarioList2QVariantList(QList<Scenario> &candidates);

 signals:
    void matchingDone(QVariantList candidates);
    
};

/* plugin */
class Q_DECL_EXPORT CurveExtensionPlugin : public QQmlExtensionPlugin {
    Q_OBJECT
    Q_PLUGIN_METADATA(IID CURVE_PLUGIN_ID)
      
 public:
    CurveExtensionPlugin();
    ~CurveExtensionPlugin();

    virtual void initializeEngine(QQmlEngine *engine, const char *uri);
    virtual void registerTypes(const char *uri);

};

#endif /* CURVE_PLUGIN_H */
