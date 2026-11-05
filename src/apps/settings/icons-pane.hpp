// The applications' icons and names: the installed applications on the
// left, and the chosen one's icon and name on the right, with buttons to give
// it another icon and a field to give it another name.
//
// The config's [app_icons] maps a desktop entry ID to a picture file's path
// (beginning with "/") or an icon theme name, and [app_names] to the name the
// desktop shows; the shell follows both (src/shell/IconSource.qml,
// src/shell/ApplicationName.js). [app_grid] hidden lists the IDs the app
// grid leaves out.

#pragma once

#include <QIcon>
#include <QList>
#include <QWidget>

#include "installed-applications.hpp"

class Choices;
class QCheckBox;
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
    // Empty if the user gave it no name of its own.
    QString chosenNameOf(const QString& id) const;
    // The name the desktop shows: the chosen one, else its own.
    QString shownNameOf(const InstalledApplication& application) const;
    bool hiddenInGrid(const QString& id) const;
    void showInGrid(const QString& id, bool shown);
    const InstalledApplication* applicationWithId(const QString& id) const;

    void askForIcon();
    void askForFile();
    // Also moves the application to the list group it now belongs to.
    void changeIcon(const QString& id, const QString& choice);
    // The field's name for the shown application; its own name, or none,
    // takes the given one back.
    void changeName(const QString& name);

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
    QLineEdit* name_field = nullptr;
    QPushButton* original_name_button = nullptr;
    QCheckBox* in_grid = nullptr;
    QString shown_id;
};
