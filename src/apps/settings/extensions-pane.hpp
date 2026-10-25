// The extensions (docs/extensions.md): a list on the left, grouped into on
// and off, and on the right the chosen one with a switch and a tick box for
// each of its labelled yes-or-no settings.

#pragma once

#include <QVariantMap>
#include <QWidget>

class Choices;
class QCheckBox;
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

    void fillList(const QString& chosen_name);
    void showExtension(const QString& name);
    bool isEnabled(const QString& name) const;
    QString titleOf(const QString& name) const;
    QIcon iconOf(const QString& name) const;
    QVariantMap manifestOf(const QString& name) const;

    void change(const QString& name, bool on);
    void changeSetting(const QString& name, const QString& key, bool on);
    void openOwnFolder() const;

    Choices& choices;
    QVariantMap words;
    QVariantMap found;

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
