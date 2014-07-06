TARGET = cli

PROJECTNAME = cli

TEMPLATE = app
CONFIG += qt debug
QT += qml quick

LIBS += -lcurveplugin

DEPENDPATH += . ..
INCLUDEPATH += . ../curve
LIBPATH += . ../curve/build

SOURCES += cli.cpp
HEADERS += ../curve/curve_match.h ../curve/tree.h

DESTDIR = build
OBJECTS_DIR = $$DESTDIR
MOC_DIR = $$DESTDIR
RCC_DIR = $$DESTDIR
UI_DIR = $$DESTDIR
