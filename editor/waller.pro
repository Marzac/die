QT += core gui spatialaudio

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++17
QMAKE_CXXFLAGS_RELEASE *= -O2
QMAKE_LFLAGS_RELEASE *= -s
QMAKE_CXXFLAGS += -msse4 -save-temps -Wall -Wextra

#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000

INCLUDEPATH += \
    ../common/engine \
    ../common/game

SOURCES += \
    main.cpp \
    editor.cpp \
    walker.cpp \
    mainwindow.cpp \
    renderwindow.cpp \
    wdgmapeditor.cpp \
    wdgmapview.cpp \
    wdgtexselector.cpp \
    wdgtexview.cpp \
    ../common/engine/renderer.cpp \
    ../common/engine/renderer_config.cpp \
    ../common/engine/audio.cpp \
    ../common/engine/map.cpp \
    ../common/engine/map_io.cpp \
    ../common/engine/tags.cpp \
    ../common/engine/env.cpp \
    ../common/engine/workerpool.cpp \
    ../common/engine/gamepad.cpp

HEADERS += \
    globals.h \
    editor.h \
    walker.h \
    mainwindow.h \
    renderwindow.h \
    wdgmapeditor.h \
    wdgmapview.h \
    wdgtexselector.h \
    wdgtexview.h \
    ../common/engine/renderer.h \
    ../common/engine/audio.h \
    ../common/engine/map.h \
    ../common/engine/tags.h \
    ../common/engine/env.h \
    ../common/engine/mapobjects.h \
    ../common/engine/mapobjects_eq.h \
    ../common/engine/easings.h \
    ../common/engine/colors.h \
    ../common/engine/postfx.h \
    ../common/engine/workerpool.h \
    ../common/engine/gamepad.h

FORMS += \
    mainwindow.ui \
    renderwindow.ui

win32:LIBS += \
    -lXinput \
    -lwinmm \
    -lws2_32

linux:LIBS += \

qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

RESOURCES += \
    resources.qrc

RC_FILE += \
    editor.rc
