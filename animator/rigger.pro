QT += core gui

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

SOURCES += \
    main.cpp \
    mainwindow.cpp \
    rigger.cpp \
    wdgrigeditor.cpp \
    ../common/engine/rig.cpp \
    ../common/engine/rig_io.cpp

HEADERS += \
    mainwindow.h \
    rigger.h \
    wdgrigeditor.h \
    ../common/engine/rig.h \
    ../common/engine/rigobjects.h

FORMS += \
    mainwindow.ui

#win32:LIBS += \
#    -lXinput \
#    -lwinmm \
#    -lws2_32
#
#linux:LIBS += \

qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

#RESOURCES += \
#    resources.qrc

#RC_FILE += \
#    rigger.rc
