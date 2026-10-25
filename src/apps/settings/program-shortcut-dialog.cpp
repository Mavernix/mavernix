#include "program-shortcut-dialog.hpp"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QProcess>
#include <QPushButton>
#include <QVBoxLayout>

#include "installed-programs.hpp"
#include "shortcut-editor.hpp"

static QString word(const QVariantMap& words, const QString& key, const QString& fallback)
{
    return words.value(key, fallback).toString();
}

ProgramShortcutDialog::ProgramShortcutDialog(
    QWidget* parent, const QVariantMap& words, const ConflictFinder& find_conflict)
    : QDialog(parent)
{
    setWindowTitle(
        word(words, QStringLiteral("program_shortcut_dialog_title"), QStringLiteral("New Program Shortcut")));
    setModal(true);

    auto* rows = new QVBoxLayout(this);
    rows->setSpacing(12);

    title_field = new QLineEdit;
    title_field->setMinimumWidth(320);

    // The first entry is for a command that is not a listed program.
    program_list = new QComboBox;
    program_list->addItem(
        word(words, QStringLiteral("program_shortcut_custom"), QStringLiteral("Other command")), QString());
    const QList<InstalledProgram> programs = installed_programs();
    for (const InstalledProgram& program : programs)
    {
        program_list->addItem(program.icon, program.name, program.command);
    }

    command_field = new QLineEdit;
    command_field->setPlaceholderText(word(words, QStringLiteral("program_shortcut_command_hint"),
        QStringLiteral("The command, such as: firefox --private-window")));

    editor = new ShortcutEditor(words);
    editor->setConflictFinder(find_conflict);
    // Starts with a shortcut that is allowed, so that the window does not
    // open with a complaint.
    editor->setShortcut(parse_shortcut(QStringLiteral("<ctrl> <alt> KEY_A")));

    auto* form = new QFormLayout;
    form->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
    form->addRow(word(words, QStringLiteral("program_shortcut_name"), QStringLiteral("Name:")), title_field);
    form->addRow(word(words, QStringLiteral("program_shortcut_program"), QStringLiteral("Program:")), program_list);
    form->addRow(word(words, QStringLiteral("program_shortcut_command"), QStringLiteral("Command:")), command_field);
    form->addRow(word(words, QStringLiteral("program_shortcut_keys"), QStringLiteral("Keys:")), editor);
    rows->addLayout(form);

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

    connect(program_list, &QComboBox::activated, this, [this](int index) {
        const QString command = program_list->itemData(index).toString();
        if (command.isEmpty())
        {
            return;
        }
        command_field->setText(command);
        // Never replaces a name the person typed.
        if (title_field->text().isEmpty() || title_field->text() == title_from_list)
        {
            title_from_list = program_list->itemText(index);
            title_field->setText(title_from_list);
        }
    });
    connect(title_field, &QLineEdit::textChanged, this, &ProgramShortcutDialog::checkInput);
    connect(command_field, &QLineEdit::textChanged, this, &ProgramShortcutDialog::checkInput);
    connect(editor, &ShortcutEditor::changed, this, &ProgramShortcutDialog::checkInput);
    checkInput();
}

void ProgramShortcutDialog::checkInput()
{
    // An unfinished name or command only keeps OK disabled; a notice is
    // shown but does not stop OK.
    const QString problem = editor->problem();
    problem_label->setText(problem.isEmpty() ? editor->notice() : problem);
    const bool complete =
        !title_field->text().trimmed().isEmpty() && !QProcess::splitCommand(command_field->text()).isEmpty();
    buttons->button(QDialogButtonBox::Ok)->setEnabled(problem.isEmpty() && complete);
}

bool ProgramShortcutDialog::ask(
    QWidget* parent, const QVariantMap& words, ProgramShortcut& made, const ConflictFinder& find_conflict)
{
    ProgramShortcutDialog dialog(parent, words, find_conflict);
    if (dialog.exec() != QDialog::Accepted)
    {
        return false;
    }
    made.title = dialog.title_field->text().trimmed();
    made.run = QProcess::splitCommand(dialog.command_field->text());
    made.shortcut = dialog.editor->shortcut();
    return true;
}
