// The extensions (docs/extensions.md): a list on the left, grouped into on
// and off, and on the right the chosen one with a switch and a tick box for
// each of its labelled yes-or-no settings.

#pragma once

#include <QVariantMap>
#include <QWidget>

class Choices;
class QCheckBox;
class QFileSystemWatcher;
class QLabel;
class QListWidget;
class QVBoxLayout;

class ExtensionsPane : public QWidget
{
    Q_OBJECT

  public:
    ExtensionsPane(Choices& choices, QWidget* parent = nullptr);

  private:
    QString word(const char* key, const char* fallback) const;
    QWidget* buildList();
    QWidget* buildDetail();
    QWidget* buildFolderButton();

    void watchFiles();
    // Which are on, as the user's choices file holds it now.
    QStringList enabledInFile() const;
    // Reads the extensions and which are on again, and shows them if they changed.
    void reload();
    // The extensions found, or that there are none.
    void showFound();
    void fillList(const QString& chosen_name);
    void showExtension(const QString& name);
    bool isEnabled(const QString& name) const;
    QString titleOf(const QString& name) const;
    QIcon iconOf(const QString& name) const;
    QVariantMap manifestOf(const QString& name) const;

    void change(const QString& name, bool on);
    void changeSetting(const QString& name, const QString& key, const QVariant& value);
    void openOwnFolder() const;

    Choices& choices;
    QVariantMap words;
    QVariantMap found;
    // The names of those switched on.
    QStringList enabled;

    QFileSystemWatcher* watcher = nullptr;
    QLabel* none = nullptr;
    // The list and the chosen one's details, while there are any.
    QWidget* some = nullptr;
    QListWidget* list = nullptr;
    QLabel* detail_icon = nullptr;
    QLabel* detail_title = nullptr;
    QLabel* detail_version = nullptr;
    QLabel* detail_description = nullptr;
    QCheckBox* detail_switch = nullptr;
    // The extension's own settings, replaced whenever another is chosen.
    QWidget* detail_settings = nullptr;
    QVBoxLayout* detail_settings_rows = nullptr;
    QString shown_name;
};
