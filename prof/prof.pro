TARGET = curveprofile

PROJECTNAME = curveprofile

TEMPLATE = app
CONFIG += qt debug
QT += qml quick

DEPENDPATH += .
INCLUDEPATH += ../curve

SOURCES += ../cli/cli.cpp ../curve/curve_match.cpp ../curve/tree.cpp ../curve/score.cpp ../curve/incr_match.cpp ../curve/functions.cpp ../curve/thread.cpp
HEADERS += ../curve/curve_match.h ../curve/tree.h ../curve/default_params.h ../curve/score.h ../curve/incr_match.h ../curve/functions.h ../curve/thread.h ../curve/log.h

DESTDIR = build
OBJECTS_DIR = $$DESTDIR
MOC_DIR = $$DESTDIR
RCC_DIR = $$DESTDIR
UI_DIR = $$DESTDIR

# qmake -config debug
QMAKE_CXXFLAGS += -pg
QMAKE_LFLAGS += -pg

