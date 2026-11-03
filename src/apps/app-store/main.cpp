// The App Store: installs the apps the Mavernix organisation publishes on
// GitHub, for the user alone. A Qt Quick window (qml/) over a Python back end
// (backend/) that does the fetching and the installing.
//
// Its look is the theme's [app_store] table; its words, and where it looks
// for apps, the desktop config's [app_store].

#include <QFile>
#include <QGuiApplication>
#include <QIcon>
#include <QJsonDocument>
#include <QJsonObject>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickImageProvider>

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

// "image://theme-icon/NAME": the icon theme's icon NAME, for an app or
// extension whose listing has none (AppIcon.qml). A pixmap provider, as Qt
// asks those on the main thread only, where QIcon may be used.
class ThemeIconProvider : public QQuickImageProvider
{
  public:
    ThemeIconProvider() : QQuickImageProvider(QQuickImageProvider::Pixmap) {}

    QPixmap requestPixmap(const QString& name, QSize* size, const QSize& requested_size) override
    {
        const QSize wanted = requested_size.isEmpty() ? QSize(128, 128) : requested_size;
        const QPixmap drawn = QIcon::fromTheme(name).pixmap(wanted);
        if (size != nullptr)
        {
            *size = drawn.size();
        }
        return drawn;
    }
};

int main(int argc, char** argv)
{
    QGuiApplication app(argc, argv);
    // The dock and the panel recognise the window by its desktop entry.
    QGuiApplication::setDesktopFileName(QStringLiteral("mavernix-app-store"));

    const QVariantMap settings = read_settings();
    const QVariantMap wishes =
        settings.value(QStringLiteral("config")).toMap().value(QStringLiteral("app_store")).toMap();
    Store store(wishes.value(QStringLiteral("organisation"), QStringLiteral("Mavernix")).toString(),
        wishes.value(QStringLiteral("topic"), QStringLiteral("mavernix-app")).toString(),
        wishes.value(QStringLiteral("extensions_repository"), QStringLiteral("mavernix-extensions")).toString());

    QQmlApplicationEngine engine;
    engine.addImageProvider(QStringLiteral("theme-icon"), new ThemeIconProvider);
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
