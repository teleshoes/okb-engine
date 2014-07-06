TARGET = dump

PROJECTNAME = dump

TEMPLATE = app
CONFIG += qt # debug
QT += qml quick

LIBS += -lcurveplugin

DEPENDPATH += . ..
INCLUDEPATH += . ../curve
LIBPATH += . ../curve/build

SOURCES += dump.cpp
HEADERS += ../curve/curve_match.h ../curve/tree.h

DESTDIR = build
OBJECTS_DIR = $$DESTDIR
MOC_DIR = $$DESTDIR
RCC_DIR = $$DESTDIR
UI_DIR = $$DESTDIR
