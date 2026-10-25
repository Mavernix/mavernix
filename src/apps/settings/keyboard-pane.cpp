#include "keyboard-pane.hpp"

#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

#include "choices.hpp"
#include "program-shortcut-dialog.hpp"
#include "shortcut-editor.hpp"

// The desktop's actions by group. Names are the config's [shortcuts]
// (src/session/make_wayfire_config.py knows them all); the title is used
// when the config's words have no "shortcut_NAME".
struct Action
{
    const char* name;
    const char* title;
};
struct Category
{
    const char* name;
    const char* title;
    QList<Action> actions;
};

static const QList<Category>& categories()
{
    static const QList<Category> list = {
        {"windows", "Windows",
            {
                {"close_window", "Close Window"},
                {"minimize_window", "Minimize Window"},
                {"fullscreen_window", "Full Screen"},
                {"switch_window", "Next Window of the Program"},
                {"snap_left", "Snap Window Left"},
                {"snap_right", "Snap Window Right"},
                {"snap_up", "Snap Window Up"},
                {"snap_down", "Snap Window Down"},
                {"maximize_window", "Fill the Screen"},
                {"restore_window", "Restore Window Size"},
                {"tile_left", "Left Half"},
                {"tile_right", "Right Half"},
                {"tile_top", "Upper Half"},
                {"tile_bottom", "Lower Half"},
                {"tile_top_left", "Upper Left Quarter"},
                {"tile_top_right", "Upper Right Quarter"},
                {"tile_bottom_left", "Lower Left Quarter"},
                {"tile_bottom_right", "Lower Right Quarter"},
            }},
        {"desktops", "Desktops",
            {
                {"window_overview", "Show All Windows"},
                {"show_desktop", "Show Desktop"},
                {"previous_desktop", "Go to the Previous Desktop"},
                {"next_desktop", "Go to the Next Desktop"},
                {"window_to_previous_desktop", "Move Window to the Previous Desktop"},
                {"window_to_next_desktop", "Move Window to the Next Desktop"},
            }},
        {"launching", "Launching",
            {
                {"app_grid", "Show All Applications"},
                {"search", "Search"},
                {"switch_program", "Next Program"},
                {"switch_program_back", "Previous Program"},
                {"notification_list", "Show Notifications"},
                {"lock", "Lock Screen"},
            }},
        {"sound", "Sound",
            {
                {"volume_up", "Louder"},
                {"volume_down", "Quieter"},
                {"volume_mute", "Mute"},
            }},
    };
    return list;
}

static const char* const programs_category = "programs";
static const char* const shortcuts_table = "shortcuts";
static const char* const commands_table = "commands";

static QString word(const QVariantMap& words, const QString& key, const QString& fallback)
{
    return words.value(key, fallback).toString();
}

// "brightness_up" -> "Brightness up", for a command the words give no title.
static QString title_from_name(const QString& name)
{
    QString title = name;
    title.replace(QLatin1Char('_'), QLatin1Char(' '));
    return title.left(1).toUpper() + title.mid(1);
}

KeyboardPane::KeyboardPane(Choices& choices, QWidget* parent) : QWidget(parent), choices(choices)
{
    words = choices.configTable(QStringLiteral("settings")).value(QStringLiteral("words")).toMap();

    category_list = new QListWidget;
    for (const Category& category : categories())
    {
        category_names << QLatin1String(category.name);
        category_list->addItem(word(
            words, QStringLiteral("keyboard_category_") + QLatin1String(category.name), QLatin1String(category.title)));
    }
    category_names << QLatin1String(programs_category);
    category_list->addItem(word(words, QStringLiteral("keyboard_category_programs"), QStringLiteral("Programs")));
    // As wide as the longest name and as tall as the table beside it.
    const int category_list_width_px = 170;
    category_list->setFixedWidth(category_list_width_px);
    category_list->setCurrentRow(0);

    table = new QTableWidget(0, 2);
    table->setHorizontalHeaderLabels({
        word(words, QStringLiteral("keyboard_column_action"), QStringLiteral("Action")),
        word(words, QStringLiteral("keyboard_column_keys"), QStringLiteral("Shortcut")),
    });
    table->verticalHeader()->hide();
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setShowGrid(false);
    table->horizontalHeader()->setStretchLastSection(false);
    table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    // No fixed width: the pane sits in a tab, whose frame takes some of the
    // window's fixed width.
    const int table_height_px = 300;
    table->setFixedHeight(table_height_px);

    change_button = new QPushButton(word(words, QStringLiteral("keyboard_change"), QStringLiteral("Change…")));
    clear_button = new QPushButton(word(words, QStringLiteral("keyboard_clear"), QStringLiteral("Clear")));
    restore_button =
        new QPushButton(word(words, QStringLiteral("keyboard_restore"), QStringLiteral("Restore Default")));
    add_button = new QPushButton(word(words, QStringLiteral("keyboard_add"), QStringLiteral("Add…")));
    remove_button = new QPushButton(word(words, QStringLiteral("keyboard_remove"), QStringLiteral("Remove")));

    auto* buttons = new QHBoxLayout;
    buttons->addWidget(add_button);
    buttons->addWidget(remove_button);
    buttons->addStretch();
    buttons->addWidget(change_button);
    buttons->addWidget(clear_button);
    buttons->addWidget(restore_button);

    // Where a key was taken from, or what went wrong.
    note_label = new QLabel;
    note_label->setWordWrap(true);
    note_label->setMinimumHeight(note_label->fontMetrics().lineSpacing() * 2);

    auto* right = new QVBoxLayout;
    right->addWidget(table);
    right->addLayout(buttons);
    right->addWidget(note_label);

    auto* columns = new QHBoxLayout(this);
    columns->addWidget(category_list, 0, Qt::AlignTop);
    columns->addLayout(right);
    category_list->setFixedHeight(table_height_px);

    connect(category_list, &QListWidget::currentRowChanged, this, &KeyboardPane::showCategory);
    connect(table, &QTableWidget::itemSelectionChanged, this, &KeyboardPane::updateButtons);
    connect(table, &QTableWidget::cellDoubleClicked, this, &KeyboardPane::changeSelected);
    connect(change_button, &QPushButton::clicked, this, &KeyboardPane::changeSelected);
    connect(clear_button, &QPushButton::clicked, this, &KeyboardPane::clearSelected);
    connect(restore_button, &QPushButton::clicked, this, &KeyboardPane::restoreSelected);
    connect(add_button, &QPushButton::clicked, this, &KeyboardPane::addProgram);
    connect(remove_button, &QPushButton::clicked, this, &KeyboardPane::removeSelected);

    showCategory();
}

QList<KeyboardPane::Entry> KeyboardPane::entriesOf(const QString& category) const
{
    QList<Entry> entries;
    if (category == QLatin1String(programs_category))
    {
        QStringList names = choices.names(QLatin1String(commands_table));
        names.sort();
        for (const QString& name : names)
        {
            entries.append({QLatin1String(commands_table), name});
        }
        return entries;
    }

    for (const Category& known : categories())
    {
        if (category == QLatin1String(known.name))
        {
            for (const Action& action : known.actions)
            {
                // Only what this desktop can do: the program switcher is a
                // part some themes have none of.
                const QString action_name = QLatin1String(action.name);
                const bool is_switcher = action_name == QLatin1String("switch_program") ||
                                         action_name == QLatin1String("switch_program_back");
                if (is_switcher && choices.themeTable(QStringLiteral("program_switcher")).isEmpty())
                {
                    continue;
                }
                entries.append({QLatin1String(shortcuts_table), QLatin1String(action.name)});
            }
        }
    }
    return entries;
}

QList<KeyboardPane::Entry> KeyboardPane::allEntries() const
{
    QList<Entry> everything = entriesOf(QLatin1String(programs_category));
    for (const Category& category : categories())
    {
        everything += entriesOf(QLatin1String(category.name));
    }
    return everything;
}

QString KeyboardPane::titleOf(const Entry& entry) const
{
    if (entry.table == QLatin1String(commands_table))
    {
        // A program shortcut made in this window carries its own title.
        const QString given = choices.value(entry.table, entry.name).toMap().value(QStringLiteral("title")).toString();
        if (!given.isEmpty())
        {
            return given;
        }
        return word(words, QStringLiteral("command_") + entry.name, title_from_name(entry.name));
    }

    for (const Category& category : categories())
    {
        for (const Action& action : category.actions)
        {
            if (entry.name == QLatin1String(action.name))
            {
                return word(words, QStringLiteral("shortcut_") + entry.name, QLatin1String(action.title));
            }
        }
    }
    return title_from_name(entry.name);
}

QString KeyboardPane::keysOf(const Entry& entry) const
{
    if (entry.table == QLatin1String(commands_table))
    {
        return choices.value(entry.table, entry.name).toMap().value(QStringLiteral("key")).toString();
    }
    return choices.value(entry.table, entry.name).toString();
}

QString KeyboardPane::defaultKeysOf(const Entry& entry) const
{
    const QVariant held = choices.defaultOf(entry.table, entry.name);
    if (entry.table == QLatin1String(commands_table))
    {
        return held.toMap().value(QStringLiteral("key")).toString();
    }
    return held.toString();
}

QString KeyboardPane::setKeys(const Entry& entry, const QString& keys)
{
    if (entry.table == QLatin1String(commands_table))
    {
        // The whole entry is written: the program to run stays as it was.
        QVariantMap command = choices.value(entry.table, entry.name).toMap();
        command.insert(QStringLiteral("key"), keys);
        return choices.set(entry.table, entry.name, command);
    }
    return choices.set(entry.table, entry.name, keys);
}

QString KeyboardPane::takenBy(const Shortcut& shortcut, const Entry& except) const
{
    for (const Entry& other : allEntries())
    {
        if (other.table == except.table && other.name == except.name)
        {
            continue;
        }
        if (has_alternative(keysOf(other), shortcut))
        {
            return titleOf(other);
        }
    }
    return QString();
}

void KeyboardPane::takeOver(const Shortcut& shortcut, const Entry& new_holder)
{
    for (const Entry& other : allEntries())
    {
        if ((other.table == new_holder.table && other.name == new_holder.name) ||
            !has_alternative(keysOf(other), shortcut))
        {
            continue;
        }
        QStringList kept;
        for (const Shortcut& alternative : parse_alternatives(keysOf(other)))
        {
            if (write_shortcut(alternative) != write_shortcut(shortcut))
            {
                kept << write_shortcut(alternative);
            }
        }
        setKeys(other, kept.join(QStringLiteral(" | ")));
    }
}

void KeyboardPane::showNote(const QString& note)
{
    note_label->setText(note);
}

void KeyboardPane::showCategory()
{
    const int row = category_list->currentRow();
    if (row < 0)
    {
        return;
    }

    const QString none_word = word(words, QStringLiteral("keyboard_none"), QStringLiteral("none"));
    shown_entries = entriesOf(category_names.value(row));
    table->clearSelection();
    table->setRowCount(int(shown_entries.size()));
    for (int index = 0; index < shown_entries.size(); index++)
    {
        const Entry& entry = shown_entries[index];
        table->setItem(index, 0, new QTableWidgetItem(titleOf(entry)));
        auto* keys_item = new QTableWidgetItem(shortcut_text(keysOf(entry), words, none_word));
        keys_item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        table->setItem(index, 1, keys_item);
    }
    showNote(QString());
    updateButtons();
}

KeyboardPane::Entry KeyboardPane::selectedEntry() const
{
    const int row = table->currentRow();
    if (row < 0 || row >= shown_entries.size() || table->selectedItems().isEmpty())
    {
        return Entry();
    }
    return shown_entries[row];
}

void KeyboardPane::updateButtons()
{
    const Entry entry = selectedEntry();
    const bool selected = !entry.name.isEmpty();

    change_button->setEnabled(selected);
    clear_button->setEnabled(selected && !keysOf(entry).isEmpty());
    restore_button->setEnabled(
        selected && keysOf(entry) != defaultKeysOf(entry) && choices.defaultOf(entry.table, entry.name).isValid());
    // Only program shortcuts the user added can be removed; the desktop's
    // own are only cleared, so that their default can be given back.
    remove_button->setEnabled(selected && entry.table == QLatin1String(commands_table) &&
                              !choices.defaultOf(entry.table, entry.name).isValid());
}

void KeyboardPane::changeSelected()
{
    const Entry entry = selectedEntry();
    if (entry.name.isEmpty())
    {
        return;
    }

    Shortcut chosen;
    const Shortcut current = parse_shortcut(keysOf(entry));
    const ConflictFinder find_conflict = [this, entry](const Shortcut& shortcut) { return takenBy(shortcut, entry); };
    if (!ShortcutDialog::ask(this, words, titleOf(entry), current, chosen, find_conflict))
    {
        return;
    }

    // Keys that another action has are not given twice: both would run.
    takeOver(chosen, entry);

    const QString problem = setKeys(entry, write_shortcut(chosen));
    const int row = table->currentRow();
    showCategory();
    table->selectRow(row);
    showNote(problem);
}

void KeyboardPane::clearSelected()
{
    const Entry entry = selectedEntry();
    if (entry.name.isEmpty())
    {
        return;
    }
    const QString problem = setKeys(entry, QString());
    const int row = table->currentRow();
    showCategory();
    table->selectRow(row);
    showNote(problem);
}

void KeyboardPane::restoreSelected()
{
    const Entry entry = selectedEntry();
    if (entry.name.isEmpty())
    {
        return;
    }

    // Another action may have taken the default keys since: they are
    // given back only if nobody has them.
    QString problem;
    const QString default_keys = defaultKeysOf(entry);
    const Shortcut first_default = parse_shortcut(default_keys);
    const QString holder = first_default.isEmpty() ? QString() : takenBy(first_default, entry);
    if (!holder.isEmpty())
    {
        problem = word(words, QStringLiteral("keyboard_restore_taken"),
            QStringLiteral("The default keys are used by “%1”. Change that first."))
                      .arg(holder);
    }
    else
    {
        problem = setKeys(entry, default_keys);
    }

    const int row = table->currentRow();
    showCategory();
    table->selectRow(row);
    showNote(problem);
}

void KeyboardPane::addProgram()
{
    // Show the programs list, where the new one will be.
    category_list->setCurrentRow(int(category_names.indexOf(QLatin1String(programs_category))));

    ProgramShortcut made;
    const ConflictFinder find_conflict = [this](const Shortcut& shortcut) { return takenBy(shortcut, Entry()); };
    if (!ProgramShortcutDialog::ask(this, words, made, find_conflict))
    {
        return;
    }
    if (!made.shortcut.isEmpty())
    {
        takeOver(made.shortcut, Entry());
    }

    // "program_" and the lowest number not yet taken.
    const QStringList taken = choices.names(QLatin1String(commands_table));
    int number = 1;
    while (taken.contains(QStringLiteral("program_%1").arg(number)))
    {
        number++;
    }
    const QString name = QStringLiteral("program_%1").arg(number);

    QVariantMap command;
    command.insert(QStringLiteral("key"), write_shortcut(made.shortcut));
    command.insert(QStringLiteral("run"), QVariant(made.run));
    command.insert(QStringLiteral("title"), made.title);
    const QString problem = choices.set(QLatin1String(commands_table), name, command);

    showCategory();
    for (int index = 0; index < shown_entries.size(); index++)
    {
        if (shown_entries[index].name == name)
        {
            table->selectRow(index);
        }
    }
    showNote(problem);
}

void KeyboardPane::removeSelected()
{
    const Entry entry = selectedEntry();
    if (entry.name.isEmpty())
    {
        return;
    }
    // The config cannot delete a key, only replace its value; a null command
    // is left out (src/session/make_wayfire_config.py).
    const QString problem = choices.set(entry.table, entry.name, QVariant());
    showCategory();
    showNote(problem);
}
