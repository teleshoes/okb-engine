#include "curve_match.h"
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
  cout << " -d : debug" << endl;
  cout << " -l <file> : log file" << endl;
  exit(1);
}


int main(int argc, char* argv[]) {
  QString input;
  QFile file;
  QString logfile;
  bool defparam = false;
  
  extern char *optarg;
  extern int optind;

  int c;
  while ((c = getopt(argc, argv, "dl:")) != -1) {
    switch (c) {
    case 'd': defparam = true; break;
    case 'l': logfile = optarg; break;
    default: usage(argv[0]); break;
    }
  }

  if (argc > optind + 1) {
    file.setFileName(argv[optind+1]);
    file.open(QFile::ReadOnly);
  } else {
    file.open(stdin, QIODevice::ReadOnly);
  }
  
  QTextStream in(&file);
  QString line = in.readLine();
  while (! line.isNull()) {
    input += line;
    line = in.readLine();
  }

  CurveMatch cm;
  cm.setDebug(true);
  if (! logfile.isNull()) {
    cm.setLogFile(logfile);
  }
  cm.loadTree(QString(argv[optind]));
  cm.fromString(input);
  if (defparam) { cm.useDefaultParameters(); }
  bool matchresult = cm.match();
  cm.log(QString("OUT: ") + cm.resultToString());
  cerr << "==> Match: " << matchresult << endl;
  QString result = cm.resultToString();
  cout << "Result: " << result.toLocal8Bit().constData() << endl;
  return 0;
}
