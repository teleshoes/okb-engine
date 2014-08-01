#include "thread.h"

CurveThread::CurveThread(QObject *parent) :
  QThread(parent)
{
  waiting = false;
  idle = false;
}

void CurveThread::clearCurve() {
  addPoint(Point(CMD_CLEAR, 0));
  first = true;
}

void CurveThread::addPoint(Point point, int timestamp) {
  QTime now = QTime::currentTime();
  if (first) {
    startTime = now;
    first = false;
  }
  mutex.lock();
  curvePending.append(CurvePoint(point, (timestamp >= 0)?timestamp:startTime.msecsTo(now)));
  if (waiting) { 
    pointsAvailable.wakeOne();
  }
  mutex.unlock();
}

void CurveThread::endCurve(int id) {
  addPoint(Point(CMD_END, id));
}

void CurveThread::stopThread() {
  addPoint(Point(CMD_QUIT, 0));
  this -> wait();
}

void CurveThread::waitForIdle() {
  mutex.lock();
  if (! idle) {
    idleCondition.wait(&mutex);
  }
  mutex.unlock();
}

void CurveThread::setMatcher(IncrementalMatch *matcher) {
  this -> matcher = matcher;
}

void CurveThread::run() {
  matcher->clearCurve();
  qDebug("thread starting ...");
  QList<CurvePoint> inProgress;
  bool started = false;

  forever {
    inProgress.clear();
    
    mutex.lock();
    if (curvePending.size() == 0) {
      waiting = true;
      setIdle(! started);
      pointsAvailable.wait(&mutex); 
    }
    waiting = false;
    setIdle(false);
    
    foreach(CurvePoint p, curvePending) {
      // make sure there is no shared information (as in QT) between these two lists
      inProgress.append(p);
    }
    curvePending.clear();
    mutex.unlock();

    /* curve points processing */
    foreach(CurvePoint point, inProgress) {
      if (point.x == CMD_CLEAR) {
	matcher->clearCurve();
	started = false;
      } else if (point.x == CMD_END) {
	int id = point.y;
	matcher->endCurve(id);
	started = false;
	// @todo push back result !
      } else if (point.x == CMD_QUIT) {
	qDebug("thread exiting ...");
	return;	
      } else {
	started = true;
	matcher->addPoint(point);
      }
    }
    
  }

}

void CurveThread::setIdle(bool value) {
  if (value and ! idle) {
    idleCondition.wakeOne(); // wake up any parent thread waiting for idle	
  }  
  idle = value;
}

