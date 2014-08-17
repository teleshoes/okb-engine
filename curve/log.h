#ifndef LOG_H
#define LOG_H

#include <iostream>
using namespace std;

/* does not like threads ? #define logdebug( ... ) { qDebug( __VA_ARGS__ ); } */

#define QSTRING2PCHAR(x) ((x).toLocal8Bit().constData())

#define logdebug( ... ) { char tmp[255]; snprintf(tmp, sizeof(tmp) - 1, __VA_ARGS__); cerr << tmp << endl; }

#define logdebug_qstring(str) { cerr << QSTRING2PCHAR(str) << endl; }

#endif /* LOG_H */