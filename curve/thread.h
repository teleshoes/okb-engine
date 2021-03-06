/* simple plumbing to use incremental curve matching while drawing 
   (we use real threads! pH34r !) */

/* @todo redo this with QT-signals & slots (e.g. like in pyotherside) instead of this old-school mutex crap */

#ifndef THREAD_H
#define THREAD_H

#include "config.h"

#ifdef THREAD

#include <QThread>
#include <QMutex>
#include <QWaitCondition>
#include <QTime>
#include <QDebug>

#define AUTO_UNLOAD_DELAY 120

#include "incr_match.h"

class ThreadCallBack {
 public:
  virtual ~ThreadCallBack();
  virtual void call(QList<ScenarioDto>) = 0;
};

class CurveThread : public QThread
{
  Q_OBJECT
  
 public:
  CurveThread(QObject *parent = 0);
    
  void clearCurve();
  void addPoint(Point point, int curve_id = -1, int timestamp = -1);
  void endOneCurve(int curve_id);
  void endCurve(int id);
  
  void stopThread();
  void waitForIdle();

  void setMatcher(IncrementalMatch *matcher);
  void setCallBack(ThreadCallBack *cb);

  void loadTree(QString fileName);

  void learn(QString word, int addValue);

protected:
  /* --- variables shared between "client" and computation thread --- */
  bool waiting;
  QList<CurvePoint> curvePending;
  QWaitCondition pointsAvailable;
  QWaitCondition idleCondition;
  QMutex mutex;
  bool idle;
  QString treFile;
	     
  QMutex learnMutex;
  QList<QPair<QString, int> > learnQueue;
  /* --- end of shared variables --- */

  IncrementalMatch *matcher;
  ThreadCallBack *callback;
  QTime startTime;
  bool first;

  void run();
  void setIdle(bool);
};

enum thread_cmd_t {
  CMD_END = -1001, CMD_CLEAR = -1002, CMD_QUIT = -1003, CMD_LOAD_TRE = - 1004, CMD_LEARN = - 1005, CMD_END_CURVE = - 1006
};

#endif /* THREAD */

#endif /* THREAD_H */
