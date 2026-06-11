QT += core gui spatialaudio

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++17
QMAKE_CXXFLAGS_DEBUG += -msse4
QMAKE_CXXFLAGS_RELEASE += -msse4
QMAKE_CXXFLAGS_RELEASE *= -O2 -s
QMAKE_CXXFLAGS += -save-temps

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
    ../common/engine/gamepad.h \
    ../common/game/gameobjects.h

FORMS += \
    mainwindow.ui \
    renderwindow.ui

LIBS += \
    -lXinput \
    -lwinmm \
    -lws2_32

qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

RESOURCES += \
    ../common/game/game.qrc \
    resources.qrc

RC_FILE += \
    editor.rc
