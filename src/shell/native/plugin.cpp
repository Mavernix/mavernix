// Makes the module's types known to QML under their QML names.

#include "plugin.hpp"

#include <QFile>
#include <QIcon>
#include <QJsonDocument>
#include <QJsonObject>
#include <QQmlEngine>
#include <qqml.h>

#include "active-window.hpp"
#include "application-menu.hpp"
#include "bluetooth-agent.hpp"
#include "desktop-aside.hpp"
#include "desktops.hpp"
#include "file-kind.hpp"
#include "folder-contents.hpp"
#include "folder-watch.hpp"
#include "held-keys.hpp"
#include "icon-files.hpp"
#include "lid-watch.hpp"
#include "main-screen.hpp"
#include "menu-registrar.hpp"
#include "session-watch.hpp"
#include "shortcut-pause.hpp"
#include "sound-pack.hpp"
#include "window-attention.hpp"
#include "window-picture-provider.hpp"
#include "window-pictures.hpp"

// The shell's own icon theme, which the user may pick apart from the
// programs' (Settings, App Style). Set here, inside the shell alone: the
// environment's QT_QPA_SYSTEM_ICON_THEME stays the programs', as the Dock
// hands its environment to every program it starts. This runs as shell.qml
// is read, before any icon is drawn.
static void use_shell_icon_theme()
{
    QFile settings(qEnvironmentVariable("DESKTOP_SHELL_SETTINGS"));
    if (!settings.open(QIODevice::ReadOnly))
    {
        return;
    }
    const QString theme =
        QJsonDocument::fromJson(settings.readAll()).object().value(QStringLiteral("shell_icon_theme")).toString();
    if (!theme.isEmpty())
    {
        QIcon::setThemeName(theme);
    }
}

void ShellNativePlugin::registerTypes(const char* uri)
{
    use_shell_icon_theme();

    const int major_version = 1;
    const int minor_version = 0;

    qmlRegisterType<ActiveWindow>(uri, major_version, minor_version, "ActiveWindow");
    qmlRegisterType<ApplicationMenu>(uri, major_version, minor_version, "ApplicationMenu");
    qmlRegisterType<BluetoothAgent>(uri, major_version, minor_version, "BluetoothAgent");
    qmlRegisterType<DesktopAside>(uri, major_version, minor_version, "DesktopAside");
    qmlRegisterType<Desktops>(uri, major_version, minor_version, "Desktops");
    qmlRegisterType<FileKind>(uri, major_version, minor_version, "FileKind");
    qmlRegisterType<FolderContents>(uri, major_version, minor_version, "FolderContents");
    qmlRegisterType<FolderWatch>(uri, major_version, minor_version, "FolderWatch");
    qmlRegisterType<HeldKeys>(uri, major_version, minor_version, "HeldKeys");
    qmlRegisterType<IconFiles>(uri, major_version, minor_version, "IconFiles");
    qmlRegisterType<LidWatch>(uri, major_version, minor_version, "LidWatch");
    qmlRegisterType<MainScreen>(uri, major_version, minor_version, "MainScreen");
    qmlRegisterType<MenuRegistrar>(uri, major_version, minor_version, "MenuRegistrar");
    qmlRegisterType<SessionWatch>(uri, major_version, minor_version, "SessionWatch");
    qmlRegisterType<ShortcutPause>(uri, major_version, minor_version, "ShortcutPause");
    qmlRegisterType<SoundPack>(uri, major_version, minor_version, "SoundPack");
    qmlRegisterType<WindowAttention>(uri, major_version, minor_version, "WindowAttention");
    qmlRegisterType<WindowPictures>(uri, major_version, minor_version, "WindowPictures");
}

void ShellNativePlugin::initializeEngine(QQmlEngine* engine, const char* uri)
{
    Q_UNUSED(uri)
    // The engine takes ownership of the providers.
    engine->addImageProvider(QStringLiteral("window-picture"), new WindowPictureProvider);
    engine->addImageProvider(QStringLiteral("svg-icon"), new SvgIconProvider);
    engine->addImageProvider(QStringLiteral("theme-icon"), new ThemeIconProvider);
    engine->addImageProvider(QStringLiteral("fitted-icon"), new FittedIconProvider);
}
