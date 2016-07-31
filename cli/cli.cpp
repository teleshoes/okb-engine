/* curve matching command line interface for tests */

#include "config.h"
#include "curve_match.h"
#include "tree.h"

#ifdef INCREMENTAL
#include "incr_match.h"
#endif /* INCREMENTAL */

#ifdef THREAD
#include "thread.h"
#endif

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
  cout << progname << " -L <tree file> <letters key> <word>    add word to user dictionary" << endl;
  cout << progname << " -D <tree file>                         dump dictionary (incl. user's)" << endl;
  cout << progname << " -G <tree file> <letters key>           get words for key" << endl;
  cout << "options:" << endl;
  cout << " -d : default parameters" << endl;
  cout << " -g : disable debug more" << endl;
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
#ifdef THREAD
  int delay = 0;
#endif /* THREAD */
  int repeat = 1;
  bool act_learn = false;
  bool act_dump = false;
  bool act_get = false;

  extern char *optarg;
  extern int optind;

  int c;
  while ((c = getopt(argc, argv, "dl:a:sgm:r:LDG")) != -1) {
    switch (c) {
    case 'a': implem = atoi(optarg); break;
    case 'd': defparam = true; break;
    case 'l': logfile = optarg; break;
    case 's': showscore = true; break;
    case 'g': debug = false; break;
#ifdef THREAD
    case 'm': delay = 1000 * atoi(optarg); break;
#endif /* THREAD */
    case 'r': repeat = atoi(optarg); break;
    case 'L': act_learn = true; break;
    case 'D': act_dump = true; break;
    case 'G': act_get = true; break;
    default: usage(argv[0]); break;
    }
  }

  if (! (argc > optind)) { usage(argv[0]); }

  if (act_learn) {
    if (! (argc > optind + 2)) { usage(argv[0]); }
  } else if (act_dump || act_get) {
    //
  } else {
    if (argc > optind + 1) {
      file.setFileName(argv[optind+1]);
      file.open(QFile::ReadOnly);
    } else if (argc > optind) {
      file.open(stdin, QIODevice::ReadOnly);
    } else {
      usage(argv[0]);
    }
  }

  QTextStream in(&file);
  QString line = in.readLine();
  while (! line.isNull()) {
    input += line;
    line = in.readLine();
  }
  file.close();

  CurveMatch *cm;
  switch (implem) {
  case 0:
    cm = new CurveMatch();
    break;
  case 1:
  case 2:
#ifdef INCREMENTAL
    // hey this is my first new :-)
    cm = new IncrementalMatch();
#else
    qDebug("Incremental support not compiled in");
    return 1;
#endif /* INCREMENTAL */
    break;
  default:
    usage(argv[0]);
  }

  cm->setDebug(debug);
  if (! logfile.isNull()) {
    cm->setLogFile(logfile);
  }
  cm->loadTree(QString(argv[optind]));

  if (act_learn) {
    cm->learn(argv[optind + 1], argv[optind + 2]);
    cm->saveUserDict();
    cout << "learn OK" << endl;
    return 0;
  } else if (act_dump) {
    cm->dumpDict();
    return 0;
  } else if (act_get) {
    char* words = cm -> getPayload((unsigned char *) argv[optind + 1]);
    cout << "Word: " << (words?words:"*not found*") << endl;
    return 0;
  }

#ifdef THREAD
  CurveThread t;
#endif /* THREAD */

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
	if (p.end_marker) {
	  cm->endOneCurve(p.curve_id);
	} else {
	  cm->addPoint(p, p.curve_id, p.t);
	}
      }
      cm->endCurve(-1);
      break;
    case 2:
#ifdef THREAD
      t.setMatcher((IncrementalMatch*) cm);
      t.start();
      t.clearCurve();
      foreach(CurvePoint p, points) {
	if (delay) { usleep(delay); }
	if (p.end_marker) {
	  t.endOneCurve(p.curve_id);
	} else {
	  t.addPoint(p, p.curve_id, p.t);
	}
      }
      t.endCurve(-1);
      qDebug("Waiting for thread ...");
      t.waitForIdle();
      qDebug("Thread is idle ...");
#else
      qDebug("Thread support not compiled in");
      return 1;
#endif /* THREAD */
      break;
    }

    if (showscore) {
      QList<ScenarioType> candidates = cm->getCandidates();
      foreach(ScenarioType s, candidates) {
	foreach(QString word, s.getWordListAsList()) {
	  cout << word.toLocal8Bit().constData() << " " << s.getScore() << endl;
	}
      }
    } else {
      cerr << "==> Match: " << (cm->getCandidates().size()) << endl;
      QString result = cm->resultToString();
      cout << "Result: " << result.toLocal8Bit().constData() << endl;
    }
  }

#ifdef THREAD
  if (implem == 2) {
    t.stopThread();
  }
#endif /* THREAD */

  delete cm; // this makes valgrind happy

  return 0;
}
