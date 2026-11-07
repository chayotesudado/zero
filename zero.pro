QT += core gui network qml quick quickcontrols2 quickshapes

CONFIG += c++17 release
TARGET = zero
TEMPLATE = app

HEADERS += \
    src/backend.h \
    src/cli.h \
    src/credentials.h \
    src/tasks.h \
    src/theme.h \
    src/todoist.h \
    src/translator.h

SOURCES += \
    src/main.cpp \
    src/backend.cpp \
    src/cli.cpp \
    src/credentials.cpp \
    src/tasks.cpp \
    src/theme.cpp \
    src/todoist.cpp \
    src/translator.cpp

RESOURCES += src/resources.qrc
