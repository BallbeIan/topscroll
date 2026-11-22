TEMPLATE = app
TARGET = topscroll
QT += core gui qml quick quickcontrols2
CONFIG += c++20 release
DEFINES += QT_DEPRECATED_WARNINGS

SOURCES += \
    src/main.cpp \
    src/backend.cpp \
    src/config.cpp \
    src/scroller.cpp \
    src/status.cpp \
    src/theme.cpp \
    src/translator.cpp

HEADERS += \
    src/backend.h \
    src/config.h \
    src/eventsink.h \
    src/inputcodes.h \
    src/scroller.h \
    src/status.h \
    src/theme.h \
    src/translator.h

win32 {
    # Raw Input + a low-level mouse hook + SendInput; see src/windaemon.h.
    SOURCES += src/windaemon.cpp src/wintouchpad.cpp
    HEADERS += src/windaemon.h src/wintouchpad.h
    LIBS += -luser32 -lhid
    DEFINES += NOMINMAX WIN32_LEAN_AND_MEAN
    VERSION = 2.0.0
    QMAKE_TARGET_DESCRIPTION = "topscroll - scroll with one finger"
    QMAKE_TARGET_PRODUCT = topscroll
    RC_ICONS = windows/topscroll.ico
} else {
    # evdev + uinput; see src/daemon.h.
    SOURCES += src/daemon.cpp src/evdev.cpp
    HEADERS += src/daemon.h src/evdev.h
}

RESOURCES += src/resources.qrc
