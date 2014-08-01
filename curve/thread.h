/* simple plumbing to use incremental curve matching while drawing 
   (we use real threads! pH34r !) */

#ifndef THREAD_H
#define THREAD_H

#include <QThread>
#include <QMutex>
#include <QWaitCondition>
#include <QTime>
#include <QDebug>


#include "incr_match.h"

class CurveThread : public QThread
{
  Q_OBJECT
  
 public:
  CurveThread(QObject *parent = 0);
    
  void clearCurve();
  void addPoint(Point point, int timestamp = -1);
  void endCurve(int id);
  
  void stopThread();
  void waitForIdle();

  void setMatcher(IncrementalMatch *matcher);

protected:
  /* --- variables shared between "client" and computation thread --- */
  bool waiting;
  QList<CurvePoint> curvePending;
  QWaitCondition pointsAvailable;
  QWaitCondition idleCondition;
  QMutex mutex;
  bool idle;
  /* --- end of shared variables --- */

  IncrementalMatch *matcher;
  QTime startTime;
  bool first;

  void run();
  void setIdle(bool);
};

enum thread_cmd_t {
  CMD_END = -1001, CMD_CLEAR = -1002, CMD_QUIT = -1003
};

#endif /* THREAD_H */
