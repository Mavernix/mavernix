// The keyboard's shortcuts: the desktop's actions and the keys that start
// programs, each changeable, removable or reset to its default.

#pragma once

#include <QStringList>
#include <QVariantMap>
#include <QWidget>

#include "shortcut-keys.hpp"

class Choices;
class QListWidget;
class QLabel;
class QPushButton;
class QTableWidget;

class KeyboardPane : public QWidget
{
    Q_OBJECT

  public:
    KeyboardPane(Choices& choices, QWidget* parent = nullptr);

  private:
    // One line of the table: either an action of the desktop (the config's
    // [shortcuts]) or a program started by a key ([commands]).
    struct Entry
    {
        QString table;
        QString name;
    };

    QString titleOf(const Entry& entry) const;
    QString keysOf(const Entry& entry) const;
    QString defaultKeysOf(const Entry& entry) const;
    // Returns what went wrong, else empty.
    QString setKeys(const Entry& entry, const QString& keys);
    // The title of another entry that has this shortcut; empty if none has.
    QString takenBy(const Shortcut& shortcut, const Entry& except) const;
    // The other entries keep any other keys they have.
    void takeOver(const Shortcut& shortcut, const Entry& new_holder);
    QList<Entry> entriesOf(const QString& category) const;
    QList<Entry> allEntries() const;

    void showCategory();
    void showNote(const QString& note);
    Entry selectedEntry() const;
    void updateButtons();
    void changeSelected();
    void clearSelected();
    void restoreSelected();
    void addProgram();
    void removeSelected();

    Choices& choices;
    QVariantMap words;
    QStringList category_names;
    QListWidget* category_list;
    QTableWidget* table;
    QList<Entry> shown_entries;
    QLabel* note_label;
    QPushButton* change_button;
    QPushButton* clear_button;
    QPushButton* restore_button;
    QPushButton* add_button;
    QPushButton* remove_button;
};
