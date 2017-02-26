#include <QFile>
#include <QString>
#include <QTextStream>

#include "log.h"

static QString logFile;

void log_line(char* txt) {
  if (! logFile.isNull()) {
    QFile file(logFile);
    if (file.open(QIODevice::Append)) {
      QTextStream out(&file);
      out << txt << "\n";
    }
    file.close();
  }
}

void log_setfile(QString fname) {
  logFile = fname;
}
