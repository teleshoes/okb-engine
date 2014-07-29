/* curve matching command line interface for tests */

#include "curve_match.h"
#include "incr_match.h"
#include "tree.h"

#include <QString>
#include <QFile>
#include <QTextStream>

#include <iostream>
using namespace std;

#include <unistd.h>
#include <stdlib.h>


static void usage(char *progname) {
  cout << "usage:" << endl;
  cout << "cat file.json | " << progname << " [<options>] <tree file>" << endl;
  cout << progname << " [<options>] <tree file> <input json>" << endl;
  cout << "options:" << endl;
  cout << " -d : default parameters" << endl;
  cout << " -D : disable debug more" << endl;
  cout << " -l <file> : log file" << endl;
  cout << " -a <nr> : implementation (0:simple, 1:incremental)" << endl;
  cout << " -s : online display scores (instead of full json)" << endl;
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
  
  extern char *optarg;
  extern int optind;

  int c;
  while ((c = getopt(argc, argv, "dl:a:sD")) != -1) {
    switch (c) {
    case 'a': implem = atoi(optarg); break;
    case 'd': defparam = true; break;
    case 'l': logfile = optarg; break;
    case 's': showscore = true; break;
    case 'D': debug = false; break;
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
  case 1: 
    // hey this is my first new :-)
    cm = new IncrementalMatch();
    break;
  case 0:
    cm = new CurveMatch();
    break;
  default:
    usage(argv[0]);
  }

  cm->setDebug(debug);
  if (! logfile.isNull()) {
    cm->setLogFile(logfile);
  }
  cm->loadTree(QString(argv[optind]));
  cm->fromString(input);
  if (defparam) { cm->useDefaultParameters(); }

  if (implem > 0) {
    // in case of incremental algorithm testing we must simulate points feeding
    QList<CurvePoint> points = cm->getCurve();
    cm->clearCurve();
    foreach(CurvePoint p, points) {
      cm->addPoint(p);
    }
  }
  cm->endCurve(-1);      

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

  return 0;
}
