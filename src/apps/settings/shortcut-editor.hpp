// Showing a shortcut to a person, and letting them make a new one: by
// pressing the keys (shortcut-recorder.hpp) or, for when that cannot be done,
// by ticking the modifiers and picking the key from a list.

#pragma once

#include <QDialog>
#include <QString>
#include <functional>
#include <QVariantMap>
#include <QWidget>

#include "shortcut-keys.hpp"

class QCheckBox;
class ShortcutRecorder;
class QComboBox;
class QLabel;
class QDialogButtonBox;

// The shortcut as a person reads it. The config's [settings.words] name the
// modifiers ("shortcut_mod_ctrl" ...), what joins the parts ("shortcut_mod_join")
// and the keys ("shortcut_key_KEY_LEFT"). Empty text gives `none_word`.
QString shortcut_text(const QString& config_text, const QVariantMap& words, const QString& none_word);

// The modifiers as marks or words ("⌃⌥", "Ctrl+Alt"); empty if there are none.
QString modifiers_text(const QStringList& modifiers, const QVariantMap& words);

// The title of the action that already has a shortcut, or empty.
using ConflictFinder = std::function<QString(const Shortcut&)>;

class ShortcutEditor : public QWidget
{
    Q_OBJECT

  public:
    ShortcutEditor(const QVariantMap& words, QWidget* parent = nullptr);

    void setShortcut(const Shortcut& shortcut);
    Shortcut shortcut() const;
    // Why the shortcut cannot be used; empty if it can.
    QString problem() const;
    // A warning that does not block it: another action loses it, programs use it.
    QString notice() const;
    void setConflictFinder(ConflictFinder finder);
    // Gives the recording button the keyboard focus, which starts recording.
    void startRecording();

  signals:
    void changed();

  private:
    QVariantMap words;
    ShortcutRecorder* recorder;
    QLabel* recorder_note;
    ConflictFinder conflict_finder;
    QList<QCheckBox*> modifier_boxes;
    QStringList modifier_names;
    QComboBox* key_list;
};

class ShortcutDialog : public QDialog
{
    Q_OBJECT

  public:
    // Returns false if the person cancelled. Pressing OK despite a conflict
    // takes the keys over from the other action.
    static bool ask(QWidget* parent, const QVariantMap& words, const QString& action_title, const Shortcut& current,
        Shortcut& chosen, const ConflictFinder& find_conflict);

  private:
    ShortcutDialog(QWidget* parent, const QVariantMap& words, const QString& action_title, const Shortcut& current,
        const ConflictFinder& find_conflict);

    ShortcutEditor* editor;
    QLabel* problem_label;
    QDialogButtonBox* buttons;
};
