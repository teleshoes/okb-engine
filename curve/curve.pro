TARGET = curveplugin

PROJECTNAME = curveplugin

PLUGIN_IMPORT_PATH = org/glop/curvekb

TEMPLATE = lib
CONFIG += qt plugin debug
QT += qml quick

DEPENDPATH += .
INCLUDEPATH += .

SOURCES += curve_plugin.cpp curve_match.cpp tree.cpp score.cpp
HEADERS += curve_plugin.h curve_match.h tree.h default_params.h score.h

DESTDIR = build
OBJECTS_DIR = $$DESTDIR
MOC_DIR = $$DESTDIR
RCC_DIR = $$DESTDIR
UI_DIR = $$DESTDIR
