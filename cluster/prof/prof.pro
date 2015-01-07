TARGET = cluster

PROJECTNAME = cluster

TEMPLATE = app
CONFIG += console debug

SOURCES += ../cluster.cpp

DESTDIR = build
OBJECTS_DIR = $$DESTDIR
MOC_DIR = $$DESTDIR
RCC_DIR = $$DESTDIR
UI_DIR = $$DESTDIR

# qmake -config debug
QMAKE_CXXFLAGS += -pg
QMAKE_LFLAGS += -pg
