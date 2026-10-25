// A window for making a shortcut that starts a program: its name, the
// program (one of those installed, or any command), and the keys.

#pragma once

#include <QDialog>
#include <QStringList>
#include <QVariantMap>

#include "shortcut-editor.hpp"
#include "shortcut-keys.hpp"

class QComboBox;
class QDialogButtonBox;
class QLabel;
class QLineEdit;

struct ProgramShortcut
{
    QString title;
    // The program and its arguments, as the config's [commands] holds them.
    QStringList run;
    Shortcut shortcut;
};

class ProgramShortcutDialog : public QDialog
{
    Q_OBJECT

  public:
    // Returns false if the person cancelled. OK takes conflicting keys over.
    static bool ask(
        QWidget* parent, const QVariantMap& words, ProgramShortcut& made, const ConflictFinder& find_conflict);

  private:
    ProgramShortcutDialog(QWidget* parent, const QVariantMap& words, const ConflictFinder& find_conflict);
    void checkInput();

    QLineEdit* title_field;
    QComboBox* program_list;
    QLineEdit* command_field;
    ShortcutEditor* editor;
    QLabel* problem_label;
    QDialogButtonBox* buttons;
    // The name the program list last put in the title field, so that
    // choosing another program replaces it but never a name the person typed.
    QString title_from_list;
};
