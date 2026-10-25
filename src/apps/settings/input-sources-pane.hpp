// The settings of the keyboard's layouts: which layouts the keyboard types
// in (American international with dead keys, German, ...), in which order,
// and the key that goes on to the next one.

#pragma once

#include <QList>
#include <QString>
#include <QVariantMap>
#include <QWidget>

#include "keyboard-layouts.hpp"

class Choices;
class QComboBox;
class QLabel;
class QListWidget;
class QPushButton;

class InputSourcesPane : public QWidget
{
    Q_OBJECT

  public:
    InputSourcesPane(Choices& choices, QWidget* parent = nullptr);

  private:
    // The sources the desktop is set to now: the user's, else the machine's.
    QList<InputSource> currentSources() const;
    void showSources(int select_row);
    void updateButtons();
    // Writes the choice to the user's file and has the compositor take it over.
    void save();
    void showNote(const QString& note);

    void addSource();
    void removeSelected();
    void moveSelected(int step);

    Choices& choices;
    QVariantMap words;
    KeyboardLayouts catalogue;
    QList<InputSource> sources;
    QListWidget* list;
    QPushButton* add_button;
    QPushButton* remove_button;
    QPushButton* up_button;
    QPushButton* down_button;
    QComboBox* switch_key_box;
    QLabel* note_label;
};
