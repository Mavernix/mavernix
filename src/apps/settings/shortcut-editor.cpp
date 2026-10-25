#include "shortcut-editor.hpp"

#include "shortcut-recorder.hpp"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

static QString word(const QVariantMap& words, const QString& key, const QString& fallback)
{
    return words.value(key, fallback).toString();
}

// "Control", "Option".
static QString modifier_name(const QVariantMap& words, const QString& modifier)
{
    static const struct
    {
        const char* modifier;
        const char* name;
    } plain[] = {
        {"ctrl", "Ctrl"},
        {"alt", "Alt"},
        {"super", "Super"},
        {"shift", "Shift"},
    };
    QString fallback = modifier;
    for (const auto& entry : plain)
    {
        if (modifier == QLatin1String(entry.modifier))
        {
            fallback = QString::fromLatin1(entry.name);
        }
    }
    return word(words, QStringLiteral("shortcut_mod_") + modifier + QStringLiteral("_name"), fallback);
}

// Without a word of its own, the modifier's plain name.
static QString modifier_mark(const QVariantMap& words, const QString& modifier)
{
    return word(words, QStringLiteral("shortcut_mod_") + modifier, modifier_name(words, modifier));
}

static QString key_label(const QVariantMap& words, const QString& key)
{
    return word(words, QStringLiteral("shortcut_key_") + key, plain_key_label(key));
}

// Whatever order the config has, so the same shortcut always reads the same.
static const QStringList reading_order = {
    QStringLiteral("ctrl"),
    QStringLiteral("alt"),
    QStringLiteral("shift"),
    QStringLiteral("super"),
};

QString modifiers_text(const QStringList& modifiers, const QVariantMap& words)
{
    QStringList parts;
    for (const QString& modifier : reading_order)
    {
        if (modifiers.contains(modifier))
        {
            parts << modifier_mark(words, modifier);
        }
    }
    return parts.join(word(words, QStringLiteral("shortcut_mod_join"), QStringLiteral("+")));
}

QString shortcut_text(const QString& config_text, const QVariantMap& words, const QString& none_word)
{
    const QString join = word(words, QStringLiteral("shortcut_mod_join"), QStringLiteral("+"));

    QStringList shown;
    for (const Shortcut& shortcut : parse_alternatives(config_text))
    {
        const QString modifiers = modifiers_text(shortcut.modifiers, words);
        shown << (modifiers.isEmpty() ? key_label(words, shortcut.key)
                                      : modifiers + join + key_label(words, shortcut.key));
    }

    return shown.isEmpty() ? none_word : shown.join(QStringLiteral(", "));
}

ShortcutEditor::ShortcutEditor(const QVariantMap& words, QWidget* parent) : QWidget(parent), words(words)
{
    auto* rows = new QVBoxLayout(this);
    rows->setContentsMargins(0, 0, 0, 0);

    recorder = new ShortcutRecorder(words);
    rows->addWidget(recorder);

    // The recorder's notes, such as that it cannot reach the desktop's keys.
    recorder_note = new QLabel;
    recorder_note->setWordWrap(true);
    recorder_note->hide();
    rows->addWidget(recorder_note);

    rows->addWidget(new QLabel(word(words, QStringLiteral("shortcut_by_hand"), QStringLiteral("Or choose by hand:"))));

    auto* row = new QHBoxLayout;
    row->setContentsMargins(0, 0, 0, 0);
    rows->addLayout(row);

    for (const QString& modifier : reading_order)
    {
        auto* box = new QCheckBox(modifier_name(words, modifier));
        connect(box, &QCheckBox::toggled, this, &ShortcutEditor::changed);
        modifier_boxes.append(box);
        modifier_names << modifier;
        row->addWidget(box);
    }

    key_list = new QComboBox;
    for (const QString& key : choosable_keys())
    {
        key_list->addItem(key_label(words, key), key);
    }
    connect(key_list, &QComboBox::activated, this, &ShortcutEditor::changed);
    row->addWidget(key_list);

    connect(recorder, &ShortcutRecorder::recorded, this, [this](const Shortcut& recorded) {
        setShortcut(recorded);
        emit changed();
    });
    connect(recorder, &ShortcutRecorder::note, this, [this](const QString& text) {
        recorder_note->setText(text);
        recorder_note->setVisible(!text.isEmpty());
    });
}

void ShortcutEditor::setConflictFinder(ConflictFinder finder)
{
    conflict_finder = std::move(finder);
}

void ShortcutEditor::startRecording()
{
    // Recording starts when the button gets the focus; for a window not
    // shown yet, that is when it is shown.
    recorder->setFocus(Qt::OtherFocusReason);
}

void ShortcutEditor::setShortcut(const Shortcut& shortcut)
{
    for (int index = 0; index < modifier_boxes.size(); index++)
    {
        modifier_boxes[index]->setChecked(shortcut.modifiers.contains(modifier_names[index]));
    }

    if (shortcut.isEmpty())
    {
        key_list->setCurrentIndex(0);
        return;
    }
    int place = key_list->findData(shortcut.key);
    if (place < 0)
    {
        // A key the list does not offer (the keypad's, say) stays as it is.
        key_list->addItem(key_label(words, shortcut.key), shortcut.key);
        place = key_list->count() - 1;
    }
    key_list->setCurrentIndex(place);
}

Shortcut ShortcutEditor::shortcut() const
{
    Shortcut shortcut;
    shortcut.key = key_list->currentData().toString();
    for (int index = 0; index < modifier_boxes.size(); index++)
    {
        if (modifier_boxes[index]->isChecked())
        {
            shortcut.modifiers << modifier_names[index];
        }
    }
    return parse_shortcut(write_shortcut(shortcut));
}

QString ShortcutEditor::problem() const
{
    const Shortcut chosen = shortcut();
    const bool has_modifier = chosen.modifiers.contains(QStringLiteral("ctrl")) ||
                              chosen.modifiers.contains(QStringLiteral("alt")) ||
                              chosen.modifiers.contains(QStringLiteral("super"));
    // A key that types or moves the cursor must not be taken from every
    // program; Shift alone does not make it safe.
    if (!has_modifier && is_typing_key(chosen.key))
    {
        return word(words, QStringLiteral("shortcut_needs_modifier"),
            QStringLiteral("Hold at least one of Ctrl, Alt or Super, or this key could no longer be typed."));
    }
    return QString();
}

QString ShortcutEditor::notice() const
{
    const Shortcut chosen = shortcut();
    if (chosen.isEmpty())
    {
        return QString();
    }

    QStringList lines;
    const QString holder = conflict_finder ? conflict_finder(chosen) : QString();
    if (!holder.isEmpty())
    {
        lines << word(words, QStringLiteral("shortcut_conflict"),
            QStringLiteral("“%1” already uses these keys; if you press OK, it loses them."))
                     .arg(holder);
    }
    if (is_common_program_key(chosen))
    {
        lines << word(words, QStringLiteral("shortcut_common_key"),
            QStringLiteral(
                "Nearly every program uses these keys itself. Using them here takes them away from every program."));
    }
    return lines.join(QLatin1Char('\n'));
}

ShortcutDialog::ShortcutDialog(QWidget* parent, const QVariantMap& words, const QString& action_title,
    const Shortcut& current, const ConflictFinder& find_conflict)
    : QDialog(parent)
{
    setWindowTitle(word(words, QStringLiteral("shortcut_dialog_title"), QStringLiteral("Change Shortcut")));
    setModal(true);

    auto* rows = new QVBoxLayout(this);
    rows->setSpacing(12);
    rows->addWidget(
        new QLabel(word(words, QStringLiteral("shortcut_dialog_prompt"), QStringLiteral("Choose the keys for “%1”:"))
                .arg(action_title)));

    editor = new ShortcutEditor(words);
    editor->setShortcut(current);
    editor->setConflictFinder(find_conflict);
    rows->addWidget(editor);

    // Under the choice, not in a pop-up, so it can be corrected in place;
    // its space is kept so the window does not jump.
    problem_label = new QLabel;
    problem_label->setWordWrap(true);
    problem_label->setMinimumHeight(problem_label->fontMetrics().lineSpacing() * 3);
    rows->addWidget(problem_label);

    buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    buttons->button(QDialogButtonBox::Ok)->setText(word(words, QStringLiteral("ok"), QStringLiteral("OK")));
    buttons->button(QDialogButtonBox::Cancel)->setText(word(words, QStringLiteral("cancel"), QStringLiteral("Cancel")));
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    rows->addWidget(buttons);

    const auto show_problem = [this]() {
        // A problem stops OK; a notice is only shown.
        const QString problem = editor->problem();
        problem_label->setText(problem.isEmpty() ? editor->notice() : problem);
        buttons->button(QDialogButtonBox::Ok)->setEnabled(problem.isEmpty());
    };
    connect(editor, &ShortcutEditor::changed, this, show_problem);
    show_problem();
}

bool ShortcutDialog::ask(QWidget* parent, const QVariantMap& words, const QString& action_title,
    const Shortcut& current, Shortcut& chosen, const ConflictFinder& find_conflict)
{
    ShortcutDialog dialog(parent, words, action_title, current, find_conflict);
    dialog.editor->startRecording();
    if (dialog.exec() != QDialog::Accepted)
    {
        return false;
    }
    chosen = dialog.editor->shortcut();
    return true;
}
