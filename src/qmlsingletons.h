#pragma once

#include <QJSEngine>
#include <QQmlEngine>
#include <QtQml/qqmlregistration.h>

#include "backend.h"
#include "theme.h"
#include "translator.h"

// The app's one Theme, Backend and Translator, as QML singletons of the Zero
// module, so QML and its tooling (qmllint, qmlls) know their types. main()
// makes them and sets `instance` on each before loading any QML.
template <typename T> T *zeroInstance(T *instance) {
    Q_ASSERT(instance);
    QJSEngine::setObjectOwnership(instance, QJSEngine::CppOwnership);
    return instance;
}

struct ThemeSingleton {
    Q_GADGET
    QML_FOREIGN(Theme)
    QML_NAMED_ELEMENT(Theme)
    QML_SINGLETON
public:
    inline static Theme *instance = nullptr;
    static Theme *create(QQmlEngine *, QJSEngine *) { return zeroInstance(instance); }
};

struct BackendSingleton {
    Q_GADGET
    QML_FOREIGN(Backend)
    QML_NAMED_ELEMENT(Backend)
    QML_SINGLETON
public:
    inline static Backend *instance = nullptr;
    static Backend *create(QQmlEngine *, QJSEngine *) { return zeroInstance(instance); }
};

struct TranslatorSingleton {
    Q_GADGET
    QML_FOREIGN(Translator)
    QML_NAMED_ELEMENT(Translator)
    QML_SINGLETON
public:
    inline static Translator *instance = nullptr;
    static Translator *create(QQmlEngine *, QJSEngine *) { return zeroInstance(instance); }
};
