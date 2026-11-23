QT += core gui spatialaudio

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++17

win32-msvc* {
    QMAKE_CXXFLAGS_RELEASE *= /O2
    QMAKE_CXXFLAGS += /W4 /MP
} else {
    QMAKE_CXXFLAGS_RELEASE *= -O2
    QMAKE_LFLAGS_RELEASE   *= -s
    QMAKE_CXXFLAGS += -msse4 -save-temps -Wall -Wextra
}

#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000

INCLUDEPATH += \
    ../common/engine

# Build version header (buildinfo.h), read from git on every build by common/version.cmake
# CMake: the one shipped with Qt (Tools/CMake_64) if present, else the one on the PATH
CMAKE_BIN = cmake
QT_TOOLS_CMAKE = $$clean_path($$[QT_INSTALL_PREFIX]/../../Tools/CMake_64/bin/cmake)
win32: QT_TOOLS_CMAKE = $${QT_TOOLS_CMAKE}.exe
exists($$QT_TOOLS_CMAKE): CMAKE_BIN = $$shell_quote($$shell_path($$QT_TOOLS_CMAKE))

VERSION_SCRIPT = $$PWD/../common/version.cmake
buildinfo.input = VERSION_SCRIPT
buildinfo.output = $$OUT_PWD/buildinfo.h
buildinfo.commands = $$CMAKE_BIN -DSOURCE_DIR=$$shell_quote($$clean_path($$PWD/..)) -DOUTPUT=$$shell_quote($$OUT_PWD/buildinfo.h) -P $$shell_quote($$VERSION_SCRIPT)
buildinfo.depends = FORCE
buildinfo.CONFIG += no_link target_predeps
buildinfo.variable_out = GENERATED_FILES
QMAKE_EXTRA_COMPILERS += buildinfo
INCLUDEPATH += $$OUT_PWD

SOURCES += \
    main.cpp \
    editor.cpp \
    walker.cpp \
    mainwindow.cpp \
    renderwindow.cpp \
    wdgmapeditor.cpp \
    wdgmapview.cpp \
    wdgsurfaceedit.cpp \
    wdgtextureselector.cpp \
    ../common/engine/renderer.cpp \
    ../common/engine/renderer_config.cpp \
    ../common/engine/audio.cpp \
    ../common/engine/map.cpp \
    ../common/engine/map_io.cpp \
    ../common/engine/striper.cpp \
    ../common/engine/tags.cpp \
    ../common/engine/env.cpp \
    ../common/engine/workerpool.cpp \
    ../common/engine/gamepad.cpp \
    wdgtextureview.cpp \
    wdgutilities.cpp

HEADERS += \
    globals.h \
    editor.h \
    walker.h \
    mainwindow.h \
    renderwindow.h \
    wdgmapeditor.h \
    wdgmapview.h \
    wdgsurfaceedit.h \
    wdgtextureselector.h \
    ../common/engine/primitives.h \
    ../common/engine/renderer.h \
    ../common/engine/audio.h \
    ../common/engine/map.h \
    ../common/engine/striper.h \
    ../common/engine/tags.h \
    ../common/engine/env.h \
    ../common/engine/mapobjects.h \
    ../common/engine/mapobjects_eq.h \
    ../common/engine/easings.h \
    ../common/engine/colors.h \
    ../common/engine/postfx.h \
    ../common/engine/workerpool.h \
    ../common/engine/gamepad.h \
    wdgtextureview.h \
    wdgutilities.h

FORMS += \
    mainwindow.ui \
    renderwindow.ui \
    wdgsurfaceedit.ui

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
