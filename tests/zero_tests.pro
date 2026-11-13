QT += core gui network qml quick quickcontrols2 testlib
CONFIG += c++17 testcase
TARGET = zero_tests
TEMPLATE = app

INCLUDEPATH += ../src

HEADERS += fakeserver.h ../src/pixelart.h ../src/backend.h ../src/credentials.h ../src/tasks.h ../src/theme.h ../src/todoist.h ../src/translator.h
SOURCES += zero_tests.cpp ../src/pixelart.cpp ../src/backend.cpp ../src/credentials.cpp ../src/tasks.cpp ../src/theme.cpp ../src/todoist.cpp ../src/translator.cpp
