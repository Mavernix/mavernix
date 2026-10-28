// Makes the module's types known to QML under their QML names.

#include "plugin.hpp"

#include <QQmlEngine>
#include <qqml.h>

#include "active-window.hpp"
#include "application-menu.hpp"
#include "desktops.hpp"
#include "file-kind.hpp"
#include "folder-contents.hpp"
#include "folder-watch.hpp"
#include "held-keys.hpp"
#include "icon-files.hpp"
#include "lid-watch.hpp"
#include "menu-registrar.hpp"
#include "session-watch.hpp"
#include "sound-pack.hpp"
#include "window-picture-provider.hpp"
#include "window-pictures.hpp"

void ShellNativePlugin::registerTypes(const char* uri)
{
    const int major_version = 1;
    const int minor_version = 0;

    qmlRegisterType<ActiveWindow>(uri, major_version, minor_version, "ActiveWindow");
    qmlRegisterType<ApplicationMenu>(uri, major_version, minor_version, "ApplicationMenu");
    qmlRegisterType<Desktops>(uri, major_version, minor_version, "Desktops");
    qmlRegisterType<FileKind>(uri, major_version, minor_version, "FileKind");
    qmlRegisterType<FolderContents>(uri, major_version, minor_version, "FolderContents");
    qmlRegisterType<FolderWatch>(uri, major_version, minor_version, "FolderWatch");
    qmlRegisterType<HeldKeys>(uri, major_version, minor_version, "HeldKeys");
    qmlRegisterType<IconFiles>(uri, major_version, minor_version, "IconFiles");
    qmlRegisterType<LidWatch>(uri, major_version, minor_version, "LidWatch");
    qmlRegisterType<MenuRegistrar>(uri, major_version, minor_version, "MenuRegistrar");
    qmlRegisterType<SessionWatch>(uri, major_version, minor_version, "SessionWatch");
    qmlRegisterType<SoundPack>(uri, major_version, minor_version, "SoundPack");
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
