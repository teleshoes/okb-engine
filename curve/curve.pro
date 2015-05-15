TARGET = curveplugin

PROJECTNAME = curveplugin

PLUGIN_IMPORT_PATH = org/glop/curvekb

TEMPLATE = lib
CONFIG += qt plugin debug
QT += qml quick

DEPENDPATH += .
INCLUDEPATH += .

SOURCES += curve_plugin.cpp curve_match.cpp multi.cpp scenario.cpp tree.cpp score.cpp functions.cpp kb_distort.cpp thread.cpp incr_match.cpp 
HEADERS += curve_plugin.h curve_match.h multi.h scenario.h tree.h score.h functions.h log.h params.h kb_distort.h config.h incr_match.h thread.h

DESTDIR = build
OBJECTS_DIR = $$DESTDIR
MOC_DIR = $$DESTDIR
RCC_DIR = $$DESTDIR
UI_DIR = $$DESTDIR

QMAKE_CXXFLAGS += -Wno-psabi -std=c++11



