TEMPLATE = app
TARGET = topscroll
QT += core gui qml quick quickcontrols2
CONFIG += c++20 release
DEFINES += QT_DEPRECATED_WARNINGS

SOURCES += \
    src/main.cpp \
    src/backend.cpp \
    src/config.cpp \
    src/daemon.cpp \
    src/evdev.cpp \
    src/scroller.cpp \
    src/status.cpp \
    src/theme.cpp \
    src/translator.cpp

HEADERS += \
    src/backend.h \
    src/config.h \
    src/daemon.h \
    src/evdev.h \
    src/eventsink.h \
    src/scroller.h \
    src/status.h \
    src/theme.h \
    src/translator.h

RESOURCES += src/resources.qrc
