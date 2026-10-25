// The applications' icons: the installed applications on the left, and the
// chosen one's icon on the right, with buttons to give it another one.
//
// The config's [app_icons] maps a desktop entry ID to a picture file's path
// (beginning with "/") or an icon theme name; the shell follows it
// (src/shell/IconSource.qml).

#pragma once

#include <QIcon>
#include <QList>
#include <QWidget>

#include "installed-applications.hpp"

class Choices;
class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;

class IconsPane : public QWidget
{
    Q_OBJECT

  public:
    IconsPane(Choices& choices, QWidget* parent = nullptr);

  private:
    QString word(const char* key, const char* fallback) const;
    QWidget* buildList();
    QWidget* buildDetail();

    // Only the applications whose name or ID holds the search text.
    void fillList(const QString& chosen_id);
    // An empty ID empties the box.
    void showApplication(const QString& id);

    // Empty if none was chosen, or if the file or theme icon is gone (the
    // shell then falls back to the application's own icon too).
    QString ownChoiceOf(const QString& id) const;
    // The icon an application shows now, and the one its desktop entry names.
    QIcon iconOf(const InstalledApplication& application) const;
    QIcon ownIconOf(const InstalledApplication& application) const;
    const InstalledApplication* applicationWithId(const QString& id) const;

    void askForIcon();
    void askForFile();
    // Also moves the application to the list group it now belongs to.
    void changeIcon(const QString& id, const QString& choice);

    Choices& choices;
    QVariantMap words;
    QList<InstalledApplication> applications;

    QLineEdit* search = nullptr;
    QListWidget* list = nullptr;
    QWidget* detail = nullptr;
    QLabel* detail_icon = nullptr;
    QLabel* detail_name = nullptr;
    QLabel* detail_id = nullptr;
    QLabel* detail_state = nullptr;
    QPushButton* choose_button = nullptr;
    QPushButton* default_button = nullptr;
    QString shown_id;
};
