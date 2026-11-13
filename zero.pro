QT += core gui network qml quick quickcontrols2 quickshapes

CONFIG += c++17 release
TARGET = zero
TEMPLATE = app

# The C++ types QML uses, as the Zero module (see src/qmlsingletons.h). The
# type info and a qmldir land in build/qml/Zero, where qmllint and qmlls
# (.qmlls.ini) find them.
CONFIG += qmltypes
INCLUDEPATH += src
QML_IMPORT_NAME = Zero
QML_IMPORT_MAJOR_VERSION = 1
QMLTYPES_FILENAME = $$OUT_PWD/qml/Zero/zero.qmltypes
QMLDIR_LINES = "module Zero" "typeinfo zero.qmltypes" "depends QtQuick"
write_file($$OUT_PWD/qml/Zero/qmldir, QMLDIR_LINES)

HEADERS += \
    src/backend.h \
    src/pixelart.h \
    src/qmlsingletons.h \
    src/cli.h \
    src/credentials.h \
    src/tasks.h \
    src/theme.h \
    src/todoist.h \
    src/translator.h

SOURCES += \
    src/main.cpp \
    src/backend.cpp \
    src/pixelart.cpp \
    src/cli.cpp \
    src/credentials.cpp \
    src/tasks.cpp \
    src/theme.cpp \
    src/todoist.cpp \
    src/translator.cpp

RESOURCES += src/resources.qrc
