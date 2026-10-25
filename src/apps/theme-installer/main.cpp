// The Theme Installer: makes the Sea Lion themes from the user's own copy of
// installer, with a window that shows each step.
//
//   mavernix-theme-installer [INSTALLER]
//
// INSTALLER (a file dropped on the program's icon) starts the window at that
// step. The work is done by backend/import_theme.py (ImportJob); the look
// and words come from the theme's and config's [theme_installer] tables,
// through the JSON file named by DESKTOP_SHELL_SETTINGS.

#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QQmlApplicationEngine>
#include <QQmlContext>

#include <cstdio>

#include "import-job.hpp"

// Reads the settings file; ends the program with a message if it cannot.
static QVariantMap read_settings()
{
    const QString path = qEnvironmentVariable("DESKTOP_SHELL_SETTINGS");
    if (path.isEmpty())
    {
        std::fprintf(stderr, "mavernix-theme-installer: DESKTOP_SHELL_SETTINGS is not set. "
                             "The program is meant to be started inside the desktop session, "
                             "which sets it (start-desktop).\n");
        std::exit(1);
    }

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
    {
        std::fprintf(stderr, "mavernix-theme-installer: cannot read the settings file %s\n", qPrintable(path));
        std::exit(1);
    }

    return QJsonDocument::fromJson(file.readAll()).object().toVariantMap();
}

int main(int argc, char** argv)
{
    QGuiApplication application(argc, argv);
    // The dock and the panel recognise the window by its desktop entry.
    QGuiApplication::setDesktopFileName(QStringLiteral("mavernix-theme-installer"));

    const QVariantMap settings = read_settings();
    const QString given = argc > 1 ? QFileInfo(QString::fromLocal8Bit(argv[1])).absoluteFilePath() : QString();

    // The check runs beside the window; the first page says it is checking.
    ImportJob job;
    job.check();

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("settings"), settings);
    engine.rootContext()->setContextProperty(QStringLiteral("job"), &job);
    engine.rootContext()->setContextProperty(QStringLiteral("givenInstaller"), given);
    engine.load(QUrl::fromLocalFile(QStringLiteral(QML_DIRECTORY "/Main.qml")));
    if (engine.rootObjects().isEmpty())
    {
        return 1;
    }
    return application.exec();
}
