// The desktop's "about this computer" window: a picture, the system's
// name, and a few rows saying what the machine is made of.
//
//   mavernix-about
//   mavernix-about --facts     prints every fact it can find and ends
//                              (for trying it on a new machine)
//
// Its look is QML (qml/Main.qml), which takes every colour, size and
// picture from the desktop theme's [about] table and every word from the
// desktop config's [about] table. Both reach it the way they reach the
// shell: as the JSON file named by DESKTOP_SHELL_SETTINGS, written by
// make_shell_config.py when the session starts.

#include <QFile>
#include <QGuiApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QQmlApplicationEngine>
#include <QQmlContext>

#include <cstdio>

#include "launcher.hpp"
#include "machine.hpp"

// Reads the settings file; ends the program with a message if it cannot.
static QVariantMap read_settings()
{
    const QString path = qEnvironmentVariable("DESKTOP_SHELL_SETTINGS");
    if (path.isEmpty())
    {
        std::fprintf(stderr, "mavernix-about: DESKTOP_SHELL_SETTINGS is not set. "
                             "The program is meant to be started inside the desktop session, "
                             "which sets it (start-desktop).\n");
        std::exit(1);
    }

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
    {
        std::fprintf(stderr, "mavernix-about: cannot read the settings file %s\n", qPrintable(path));
        std::exit(1);
    }

    return QJsonDocument::fromJson(file.readAll()).object().toVariantMap();
}

// Prints every fact with its name, one a line.
static void print_facts(const Machine& machine)
{
    const char* names[] = {
        "os_name", "os_version", "kernel", "host_name", "computer", "processor", "memory", "graphics", "startup_disk"};
    for (const char* name : names)
    {
        std::printf("%-13s %s\n", name, qPrintable(machine.fact(QString::fromLatin1(name))));
    }
}

int main(int argc, char** argv)
{
    Machine machine;
    if (argc == 2 && QString::fromLocal8Bit(argv[1]) == QStringLiteral("--facts"))
    {
        print_facts(machine);
        return 0;
    }

    QGuiApplication application(argc, argv);
    // The name by which the dock and the panel recognise the program's
    // window: that of its desktop entry.
    QGuiApplication::setDesktopFileName(QStringLiteral("mavernix-about"));

    const QVariantMap settings = read_settings();

    Launcher launcher;

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("launcher"), &launcher);
    engine.rootContext()->setContextProperty(QStringLiteral("settings"), settings);
    engine.rootContext()->setContextProperty(QStringLiteral("machine"), &machine);
    engine.load(QStringLiteral(QML_DIRECTORY "/Main.qml"));
    if (engine.rootObjects().isEmpty())
    {
        std::fprintf(stderr, "mavernix-about: could not load %s/Main.qml (messages above)\n", QML_DIRECTORY);
        return 1;
    }

    return application.exec();
}
