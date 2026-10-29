// The App Store: installs the apps the Mavernix organisation publishes on
// GitHub, for the user alone. A Qt Quick window (qml/) over a Python back end
// (backend/) that does the fetching and the installing.
//
// Its look is the theme's [app_store] table; its words, and where it looks
// for apps, the desktop config's [app_store].

#include <QFile>
#include <QGuiApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QQmlApplicationEngine>
#include <QQmlContext>

#include <cstdio>
#include <cstdlib>

#include "store.hpp"

// The desktop's settings, as the session wrote them (make_shell_config.py).
static QVariantMap read_settings()
{
    const QString path = qEnvironmentVariable("DESKTOP_SHELL_SETTINGS");
    if (path.isEmpty())
    {
        std::fprintf(stderr, "mavernix-app-store: DESKTOP_SHELL_SETTINGS is not set. The program is meant to be "
                             "started inside the desktop session, which sets it (start-desktop).\n");
        std::exit(1);
    }

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
    {
        std::fprintf(stderr, "mavernix-app-store: cannot read the settings file %s\n", qPrintable(path));
        std::exit(1);
    }
    return QJsonDocument::fromJson(file.readAll()).object().toVariantMap();
}

int main(int argc, char** argv)
{
    QGuiApplication app(argc, argv);
    // The dock and the panel recognise the window by its desktop entry.
    QGuiApplication::setDesktopFileName(QStringLiteral("mavernix-app-store"));

    const QVariantMap settings = read_settings();
    const QVariantMap wishes =
        settings.value(QStringLiteral("config")).toMap().value(QStringLiteral("app_store")).toMap();
    Store store(wishes.value(QStringLiteral("organisation"), QStringLiteral("Mavernix")).toString(),
        wishes.value(QStringLiteral("topic"), QStringLiteral("mavernix-app")).toString());

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("settings"), settings);
    engine.rootContext()->setContextProperty(QStringLiteral("store"), &store);
    engine.load(QStringLiteral(QML_DIRECTORY "/Main.qml"));
    if (engine.rootObjects().isEmpty())
    {
        std::fprintf(stderr, "mavernix-app-store: could not load %s/Main.qml (messages above)\n", QML_DIRECTORY);
        return 1;
    }
    return app.exec();
}
