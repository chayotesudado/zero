// zero — Todoist for Omarchy: today, upcoming, projects, and quick add,
// from the keyboard.

#include <QCoreApplication>
#include <QGuiApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QUrl>

#include "backend.h"
#include "cli.h"
#include "credentials.h"
#include "theme.h"
#include "translator.h"

static const char *version = "0.3.4";

int main(int argc, char *argv[]) {
    QStringList arguments;
    for (int i = 0; i < argc; ++i)
        arguments << QString::fromLocal8Bit(argv[i]);

    Credentials::migrateLegacy();

    // The bar plugin runs these every few minutes, so they stay off the GUI
    // stack entirely and need no display.
    if (Cli::handles(arguments)) {
        QCoreApplication app(argc, argv);
        app.setApplicationName("zero");
        app.setApplicationVersion(version);
        return Cli::run(arguments);
    }

    QGuiApplication app(argc, argv);
    app.setApplicationName("zero");
    app.setApplicationDisplayName("Zero");
    app.setApplicationVersion(version);

    // Matches the window to zero.desktop, so the compositor (Wayland
    // app_id) and the launcher pick up the installed icon.
    app.setDesktopFileName("zero");
    app.setWindowIcon(QIcon::fromTheme("zero"));

    QQuickStyle::setStyle("Material");

    // Installed before anything is translated, and every qsTr() binding is
    // re-evaluated when the Omarchy language changes.
    Translator translator;
    app.installTranslator(&translator);

    Theme theme;
    Backend backend;

    QQmlApplicationEngine engine;
    QObject::connect(&translator, &Translator::changed, &engine, &QQmlEngine::retranslate);
    engine.rootContext()->setContextProperty("translator", &translator);
    engine.rootContext()->setContextProperty("theme", &theme);
    engine.rootContext()->setContextProperty("backend", &backend);
    engine.load(QUrl("qrc:/Main.qml"));
    if (engine.rootObjects().isEmpty())
        return 1;

    return app.exec();
}
