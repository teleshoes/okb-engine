TARGET = curveplugin

PROJECTNAME = curveplugin

PLUGIN_IMPORT_PATH = org/glop/curvekb

TEMPLATE = lib
CONFIG += qt plugin debug
QT += qml quick

DEPENDPATH += .
INCLUDEPATH += .

SOURCES += curve_plugin.cpp curve_match.cpp scenario.cpp tree.cpp score.cpp incr_match.cpp functions.cpp thread.cpp kb_distort.cpp
HEADERS += curve_plugin.h curve_match.h scenario.h tree.h score.h incr_match.h functions.h thread.h log.h params.h kb_distort.h

DESTDIR = build
OBJECTS_DIR = $$DESTDIR
MOC_DIR = $$DESTDIR
RCC_DIR = $$DESTDIR
UI_DIR = $$DESTDIR

QMAKE_CXXFLAGS += -Wno-psabi
