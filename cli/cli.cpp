/* curve matching command line interface for tests */

#include "curve_match.h"
#include "incr_match.h"
#include "tree.h"
#include "thread.h"

#include <QString>
#include <QFile>
#include <QTextStream>

#include <iostream>
using namespace std;

#include <unistd.h>
#include <stdlib.h>
#include <unistd.h>

static void usage(char *progname) {
  cout << "usage:" << endl;
  cout << "cat file.json | " << progname << " [<options>] <tree file>" << endl;
  cout << progname << " [<options>] <tree file> <input json>" << endl;
  cout << "options:" << endl;
  cout << " -d : default parameters" << endl;
  cout << " -D : disable debug more" << endl;
  cout << " -l <file> : log file" << endl;
  cout << " -a <nr> : implementation (0:simple, 1:incremental, 2:thread)" << endl;
  cout << " -s : online display scores (instead of full json)" << endl;
  cout << " -m <ms> : delay between curve point feeding (thread mode only)" << endl;
  cout << " -r <count> : repeat (for profiling)" << endl;
  exit(1);
}


int main(int argc, char* argv[]) {
  QString input;
  QFile file;
  QString logfile;
  bool defparam = false;
  int implem = 0;
  bool showscore = false;
  bool debug = true;
  int delay = 0;
  int repeat = 1;
  
  extern char *optarg;
  extern int optind;

  int c;
  while ((c = getopt(argc, argv, "dl:a:sDm:r:")) != -1) {
    switch (c) {
    case 'a': implem = atoi(optarg); break;
    case 'd': defparam = true; break;
    case 'l': logfile = optarg; break;
    case 's': showscore = true; break;
    case 'D': debug = false; break;
    case 'm': delay = 1000 * atoi(optarg); break;
    case 'r': repeat = atoi(optarg); break;
    default: usage(argv[0]); break;
    }
  }

  if (argc > optind + 1) {
    file.setFileName(argv[optind+1]);
    file.open(QFile::ReadOnly);
  } else if (argc > optind) {
    file.open(stdin, QIODevice::ReadOnly);
  } else {
    usage(argv[0]);
  }
  
  QTextStream in(&file);
  QString line = in.readLine();
  while (! line.isNull()) {
    input += line;
    line = in.readLine();
  }

  CurveMatch *cm;
  switch (implem) {
  case 0:
    cm = new CurveMatch();
    break;
  case 1: 
  case 2:
    // hey this is my first new :-)
    cm = new IncrementalMatch();
    break;
  default:
    usage(argv[0]);
  }

  cm->setDebug(debug);
  if (! logfile.isNull()) {
    cm->setLogFile(logfile);
  }
  cm->loadTree(QString(argv[optind]));

  CurveThread t;

  if (implem == 2) { repeat = 1; }

  for (int i = 0; i < repeat; i ++) {
    cm->clearCurve();
    cm->fromString(input);
    if (defparam) { cm->useDefaultParameters(); }
    
    QList<CurvePoint> points = cm->getCurve();
    
    switch (implem) {
    case 0:
    case 1:
      // in case of incremental algorithm testing we must simulate points feeding
      // also useful for testing smoothing implemented in addPoint
      cm->clearCurve();
      foreach(CurvePoint p, points) {
	cm->addPoint(p, p.t);
      }
      cm->endCurve(-1);      
      break;
    case 2:
      t.setMatcher((IncrementalMatch*) cm);
      t.start();
      t.clearCurve();
      foreach(CurvePoint p, points) {
	if (delay) { usleep(delay); }
	t.addPoint(p, p.t);
      }
      t.endCurve(-1);
      qDebug("Waiting for thread ...");
      t.waitForIdle();
      qDebug("Thread is idle ...");
      break;
    }
    
    cm->log(QString("OUT: ") + cm->resultToString());
    if (showscore) {
      QList<Scenario> candidates = cm->getCandidates();
      foreach(Scenario s, candidates) {
	cout << s.getName().toLocal8Bit().constData() << " " << s.getScore() << endl;
      }
    } else {
      cerr << "==> Match: " << (cm->getCandidates().size()) << endl;
      QString result = cm->resultToString();
      cout << "Result: " << result.toLocal8Bit().constData() << endl;
    }
  }

  if (implem == 2) {
    t.stopThread();
  }

  return 0;
}
