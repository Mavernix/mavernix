// The entry point QML calls when a file says `import Shell.Native`.

#pragma once

#include <QQmlExtensionPlugin>

class ShellNativePlugin : public QQmlExtensionPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID QQmlExtensionInterface_iid)

  public:
    void registerTypes(const char* uri) override;
    void initializeEngine(QQmlEngine* engine, const char* uri) override;
};
