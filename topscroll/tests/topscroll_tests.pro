TEMPLATE = app
TARGET = topscroll_tests
QT += core testlib
CONFIG += c++20 console testcase
CONFIG -= app_bundle

# The sources are compiled in directly, so the tests need no installed binary
# and never touch a real input device.
SOURCES += \
    topscroll_tests.cpp \
    ../src/config.cpp \
    ../src/scroller.cpp \
    ../src/status.cpp \
    ../src/theme.cpp \
    ../src/translator.cpp

HEADERS += \
    ../src/config.h \
    ../src/eventsink.h \
    ../src/scroller.h \
    ../src/status.h \
    ../src/theme.h \
    ../src/translator.h
