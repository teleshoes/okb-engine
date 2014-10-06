#include "thread.h"
#include "log.h"

#include <sys/time.h>
#include <sys/resource.h>
#include <stdio.h>

static float cpu_user(struct rusage &start, struct rusage &stop) {
  return (float) (stop.ru_utime.tv_sec - start.ru_utime.tv_sec) +    
    (float) (stop.ru_utime.tv_usec - start.ru_utime.tv_usec) / 1000000.0;
}

static float cpu_sys(struct rusage &start, struct rusage &stop) {
  return (float) (stop.ru_stime.tv_sec - start.ru_stime.tv_sec) +    
    (float) (stop.ru_stime.tv_usec - start.ru_stime.tv_usec) / 1000000.0;
}

// avoid compiler warning: deleting object of polymorphic class type 'XXX' 
// which has non-virtual destructor might cause undefined behaviour
ThreadCallBack::~ThreadCallBack() {}

CurveThread::CurveThread(QObject *parent) :
  QThread(parent)
{
  waiting = false;
  idle = false;
  callback = NULL;
  matcher = NULL;
}

void CurveThread::clearCurve() {
  addPoint(Point(CMD_CLEAR, 0));
  first = true;
}

void CurveThread::addPoint(Point point, int timestamp) {
  if (! isRunning()) { start(); }
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

void CurveThread::loadTree(QString fileName) {
  mutex.lock();
  treFile = fileName;
  mutex.unlock();
  addPoint(Point(CMD_LOAD_TRE, 0));
}


void CurveThread::stopThread() {
  if (isRunning()) {
    addPoint(Point(CMD_QUIT, 0));
    this -> wait();
  }
}

void CurveThread::waitForIdle() {
  /* this is mostly for tests, because in real life a user may start a new word swipe even before the previous one is matched.
     real production application must use callback feature */
  if (! isRunning()) { return; }
  mutex.lock();
  if (! idle) {
    idleCondition.wait(&mutex);
  }
  mutex.unlock();
}

void CurveThread::setMatcher(IncrementalMatch *matcher) {
  this -> matcher = matcher;
}

void CurveThread::setCallBack(ThreadCallBack *cb) {
  callback = cb;
}

void CurveThread::run() {
  logdebug("Thread starting ...");
  matcher->clearCurve();
  QList<CurvePoint> inProgress;
  bool started = false;
  bool tre_ok = false;

  /* bool try_aggressive_match = false; */

  /* statistics & counters */
  struct rusage ru_started;
  struct rusage ru_completed;
  struct rusage ru_matched;
  QTime t_completed;
  QTime t_matched;

  forever {
    inProgress.clear();
    
    /* critical section */
    mutex.lock();
    if ((curvePending.size() == 0) /* && (! try_aggressive_match) */){
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
    /* critical section end */

    /*
    if (inProgress.size() == 0 && try_aggressive_match) {
      // we are waiting for user points, so lets burn a few cpu cycle with the aggressive match (less efficient, but may get results sooner)
      matcher->aggressiveMatch();
      try_aggressive_match = false;
    }
    */

    /* curve points processing */
    foreach(CurvePoint point, inProgress) {
      /* try_aggressive_match = true; */
      if (point.x == CMD_LOAD_TRE) {
	mutex.lock();
	QString file = treFile;
	mutex.unlock();

	logdebug("loading tree: %s ...", QSTRING2PCHAR(file));
	tre_ok = matcher->loadTree(file); // status ignored for now
	matcher->clearCurve();
	started = false; // don't block the thread with a non-idle condition :-)

      } else if (! tre_ok) {
	// if .tre file is not loaded, none of the following will work, so just skip them
	started = false; // don't block the thread with a non-idle condition :-)

      } else if (point.x == CMD_CLEAR) {
	matcher->clearCurve();
	started = false;

      } else if (point.x == CMD_END) {
	int id = point.y;

	t_completed = QTime::currentTime();
	getrusage(RUSAGE_THREAD, &ru_completed);

	matcher->endCurve(id);

	t_matched = QTime::currentTime();
	getrusage(RUSAGE_THREAD, &ru_matched);
	
	// display statistics
	QList<CurvePoint> curve = matcher->getCurve();
	logdebug("+++ CPU time before=%.3f/%.3f after=%.3f/%.3f / draw-time=%.3f end-time=%.3f",
		 cpu_user(ru_started, ru_completed), cpu_sys(ru_started, ru_completed),
		 cpu_user(ru_completed, ru_matched), cpu_sys(ru_completed, ru_matched),
		 ((float)(curve[curve.size() - 1].t - curve[0].t)) / 1000,
		 (float)(t_completed.msecsTo(t_matched)) / 1000);
	
	started = false;
	if (callback) { callback->call(matcher->getCandidates()); }

      } else if (point.x == CMD_QUIT) {
	logdebug("thread exiting ...");
	mutex.lock();
	setIdle(true);
	mutex.unlock();
	return;	

      } else {
	if (! started) {
	  getrusage(RUSAGE_THREAD, &ru_started);
	}
	started = true;
	matcher->addPoint(point);
      }
    }
    
  }

}

void CurveThread::setIdle(bool value) {
  // this method must be called within a mutex.lock()
  if (value and ! idle) {
    idleCondition.wakeAll(); // wake up any parent thread waiting for idle	
  }  
  idle = value;
}

