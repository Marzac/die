QT += core gui spatialaudio
CONFIG += c++17 console
CONFIG -= app_bundle
TARGET = snap

ENGINE = $$PWD/../../common/engine
INCLUDEPATH += $$ENGINE $$PWD/../../editor

win32-msvc*: QMAKE_CXXFLAGS += /MP
else: QMAKE_CXXFLAGS += -msse4
win32: LIBS += -lxinput

SOURCES += \
    snap.cpp \
    $$ENGINE/renderer.cpp \
    $$ENGINE/renderer_config.cpp \
    $$ENGINE/audio.cpp \
    $$ENGINE/map.cpp \
    $$ENGINE/map_io.cpp \
    $$ENGINE/striper.cpp \
    $$ENGINE/tags.cpp \
    $$ENGINE/env.cpp \
    $$ENGINE/workerpool.cpp \
    $$ENGINE/gamepad.cpp
