#ifndef LOG_H
#define LOG_H

#include <iostream>
#include "time.h"

using namespace std;

/* does not like threads ? #define logdebug( ... ) { qDebug( __VA_ARGS__ ); } */

void log_line(char*);
void log_setfile(QString);

#define QSTRING2PCHAR(x) ((x).toUtf8().constData())
#define QSTRING2PUCHAR(x) (unsigned char*) ((x).toUtf8().constData())

#define logdebug( ... ) { char tmp[512]; snprintf(tmp, sizeof(tmp) - 1, __VA_ARGS__); cerr << tmp << endl; log_line(tmp); }

#define logdebug_ts( ... ) { time_t now = time(NULL); char tmp[512] = ""; \
    strcat(tmp, "["); \
    strcat(tmp, ctime(&now)); \
    tmp[strlen(tmp) - 1] = '\0'; \
    strcat(tmp, "] "); \
    int l = strlen(tmp) ; \
    snprintf(tmp + l, sizeof(tmp) - l - 1, __VA_ARGS__);	\
    cerr << tmp << endl; log_line(tmp); }

#define logdebug_qstring(str) { cerr << QSTRING2PCHAR(str) << endl; }

#endif /* LOG_H */
