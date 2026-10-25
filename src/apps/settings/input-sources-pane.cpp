#include "input-sources-pane.hpp"

#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QProcess>
#include <QPushButton>
#include <QVBoxLayout>

#include "choices.hpp"

// The session's command that writes the compositor's settings anew from
// the config and the user's choices; the compositor takes the new keyboard
// at once (see trackpad-pane.cpp).
static const QString reconfigure_command = QStringLiteral("mavernix-reconfigure");

// The keys that go on to the next layout, by the config's name for each
// (src/session/make_wayfire_config.py LAYOUT_SWITCH_OPTIONS) and what each is
// called when the config's words have none.
struct SwitchKey
{
    const char* name;
    const char* title;
};
static const QList<SwitchKey>& switch_keys()
{
    static const QList<SwitchKey> list = {
        {"ctrl_space", "Ctrl+Space"},
        {"alt_shift", "Alt+Shift"},
        {"caps_lock", "Caps Lock"},
    };
    return list;
}

// (The fallbacks hold UTF-8 text such as "…", so they are not Latin-1 strings.)
static QString word(const QVariantMap& words, const char* key, const char* fallback)
{
    return words.value(QLatin1String(key), QString::fromUtf8(fallback)).toString();
}

// Asks which source to add. Returns false if the person gave up.
static bool choose_source(QWidget* parent, const QVariantMap& words, const KeyboardLayouts& catalogue,
    const QList<InputSource>& already_there, InputSource& chosen)
{
    QDialog dialog(parent);
    dialog.setWindowTitle(word(words, "keyboard_add_source_title", "Add Input Source"));

    auto* filter = new QLineEdit;
    filter->setPlaceholderText(word(words, "keyboard_source_filter", "Search"));
    filter->setClearButtonEnabled(true);

    auto* choices = new QListWidget;
    // Wide enough for the longest names ("English (US, intl., with dead keys)")
    // and tall enough to see a good number of them.
    const int list_width_px = 380;
    const int list_height_px = 260;
    choices->setFixedSize(list_width_px, list_height_px);
    for (const InputSource& source : catalogue.all())
    {
        if (already_there.contains(source))
        {
            continue;
        }
        auto* item = new QListWidgetItem(catalogue.describe(source));
        item->setData(Qt::UserRole, source.layout);
        item->setData(Qt::UserRole + 1, source.variant);
        choices->addItem(item);
    }

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    buttons->button(QDialogButtonBox::Ok)->setText(word(words, "keyboard_add_source_ok", "Add"));
    buttons->button(QDialogButtonBox::Ok)->setEnabled(false);

    auto* rows = new QVBoxLayout(&dialog);
    rows->addWidget(filter);
    rows->addWidget(choices);
    rows->addWidget(buttons);

    QObject::connect(filter, &QLineEdit::textChanged, &dialog, [choices](const QString& text) {
        for (int row = 0; row < choices->count(); row++)
        {
            QListWidgetItem* item = choices->item(row);
            item->setHidden(!item->text().contains(text, Qt::CaseInsensitive));
        }
    });
    QObject::connect(choices, &QListWidget::currentItemChanged, &dialog,
        [buttons](QListWidgetItem* item) { buttons->button(QDialogButtonBox::Ok)->setEnabled(item != nullptr); });
    QObject::connect(choices, &QListWidget::itemDoubleClicked, &dialog, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    filter->setFocus();
    if (dialog.exec() != QDialog::Accepted || !choices->currentItem())
    {
        return false;
    }
    chosen.layout = choices->currentItem()->data(Qt::UserRole).toString();
    chosen.variant = choices->currentItem()->data(Qt::UserRole + 1).toString();
    return true;
}

InputSourcesPane::InputSourcesPane(Choices& choices, QWidget* parent) : QWidget(parent), choices(choices)
{
    words = choices.configTable(QStringLiteral("settings")).value(QStringLiteral("words")).toMap();

    QString catalogue_problem;
    catalogue = KeyboardLayouts::read(KeyboardLayouts::systemFiles(), &catalogue_problem);
    sources = currentSources();

    list = new QListWidget;
    const int list_width_px = 340;
    const int list_height_px = 200;
    list->setFixedSize(list_width_px, list_height_px);

    add_button = new QPushButton(word(words, "keyboard_source_add", "Add…"));
    remove_button = new QPushButton(word(words, "keyboard_source_remove", "Remove"));
    up_button = new QPushButton(word(words, "keyboard_source_up", "Move Up"));
    down_button = new QPushButton(word(words, "keyboard_source_down", "Move Down"));

    auto* buttons = new QVBoxLayout;
    buttons->addWidget(add_button);
    buttons->addWidget(remove_button);
    buttons->addSpacing(10);
    buttons->addWidget(up_button);
    buttons->addWidget(down_button);
    buttons->addStretch();

    auto* list_row = new QHBoxLayout;
    list_row->addWidget(list, 0, Qt::AlignTop);
    list_row->addLayout(buttons);
    list_row->addStretch();

    switch_key_box = new QComboBox;
    for (const SwitchKey& key : switch_keys())
    {
        const QString title =
            words.value(QStringLiteral("keyboard_switch_") + QLatin1String(key.name), QString::fromUtf8(key.title))
                .toString();
        switch_key_box->addItem(title, QLatin1String(key.name));
    }
    switch_key_box->addItem(word(words, "keyboard_switch_none", "None"), QString());
    const QString held_key = choices.value(QStringLiteral("input"), QStringLiteral("keyboard_switch_key")).toString();
    switch_key_box->setCurrentIndex(qMax(0, switch_key_box->findData(held_key)));

    auto* switch_row = new QHBoxLayout;
    switch_row->addWidget(new QLabel(word(words, "keyboard_switch_label", "Switch to the next source with:")));
    switch_row->addWidget(switch_key_box);
    switch_row->addStretch();

    // What went wrong, or that the first source is the one that starts.
    note_label = new QLabel;
    note_label->setWordWrap(true);
    note_label->setMinimumHeight(note_label->fontMetrics().lineSpacing() * 2);

    auto* rows = new QVBoxLayout(this);
    rows->addLayout(list_row);
    rows->addLayout(switch_row);
    rows->addWidget(note_label);

    connect(list, &QListWidget::currentRowChanged, this, &InputSourcesPane::updateButtons);
    connect(add_button, &QPushButton::clicked, this, &InputSourcesPane::addSource);
    connect(remove_button, &QPushButton::clicked, this, &InputSourcesPane::removeSelected);
    connect(up_button, &QPushButton::clicked, this, [this]() { moveSelected(-1); });
    connect(down_button, &QPushButton::clicked, this, [this]() { moveSelected(1); });
    connect(switch_key_box, &QComboBox::activated, this, &InputSourcesPane::save);

    showSources(0);
    if (!catalogue_problem.isEmpty())
    {
        add_button->setEnabled(false);
        showNote(catalogue_problem);
    }
}

QList<InputSource> InputSourcesPane::currentSources() const
{
    const QString held_layouts = choices.value(QStringLiteral("input"), QStringLiteral("keyboard_layout")).toString();
    if (!held_layouts.isEmpty())
    {
        return parse_sources(
            held_layouts, choices.value(QStringLiteral("input"), QStringLiteral("keyboard_variant")).toString());
    }

    // Nothing chosen: the session uses the machine's keyboard.
    const QVariantMap machine = choices.systemKeyboard();
    const QList<InputSource> from_machine = parse_sources(
        machine.value(QStringLiteral("layout")).toString(), machine.value(QStringLiteral("variant")).toString());
    // The compositor's own default, when the machine says nothing either.
    if (from_machine.isEmpty())
    {
        return {InputSource{QStringLiteral("us"), QString()}};
    }
    return from_machine;
}

void InputSourcesPane::showSources(int select_row)
{
    list->clear();
    for (const InputSource& source : sources)
    {
        list->addItem(catalogue.describe(source));
    }
    list->setCurrentRow(qBound(0, select_row, int(sources.size()) - 1));
    updateButtons();
}

void InputSourcesPane::updateButtons()
{
    const int row = list->currentRow();
    const bool selected = row >= 0;
    // One source always stays: a keyboard with no layout types nothing.
    remove_button->setEnabled(selected && sources.size() > 1);
    up_button->setEnabled(selected && row > 0);
    down_button->setEnabled(selected && row < sources.size() - 1);
    // With one source there is nothing to switch to.
    switch_key_box->setEnabled(sources.size() > 1);
}

void InputSourcesPane::showNote(const QString& note)
{
    note_label->setText(note);
}

void InputSourcesPane::save()
{
    const QString layouts = layouts_text(sources);
    const QString variants = variants_text(sources);
    const QString switch_key = switch_key_box->currentData().toString();

    QString problem = choices.set(QStringLiteral("input"), QStringLiteral("keyboard_layout"), layouts);
    if (problem.isEmpty())
    {
        problem = choices.set(QStringLiteral("input"), QStringLiteral("keyboard_variant"), variants);
    }
    if (problem.isEmpty())
    {
        problem = choices.set(QStringLiteral("input"), QStringLiteral("keyboard_switch_key"), switch_key);
    }
    if (!problem.isEmpty())
    {
        showNote(problem);
        return;
    }

    QProcess::startDetached(reconfigure_command, {});
    showNote(sources.size() > 1
                 ? word(words, "keyboard_sources_note", "The first source is the one the keyboard starts with.")
                 : QString());
}

void InputSourcesPane::addSource()
{
    InputSource chosen;
    if (!choose_source(this, words, catalogue, sources, chosen))
    {
        return;
    }
    sources.append(chosen);
    showSources(int(sources.size()) - 1);
    save();
}

void InputSourcesPane::removeSelected()
{
    const int row = list->currentRow();
    if (row < 0 || sources.size() < 2)
    {
        return;
    }
    sources.removeAt(row);
    showSources(row);
    save();
}

void InputSourcesPane::moveSelected(int step)
{
    const int row = list->currentRow();
    const int target = row + step;
    if (row < 0 || target < 0 || target >= sources.size())
    {
        return;
    }
    sources.swapItemsAt(row, target);
    showSources(target);
    save();
}
