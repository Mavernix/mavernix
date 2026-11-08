#include "toolkits-pane.hpp"

#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QFormLayout>
#include <QProcess>
#include <QTimer>

#include <algorithm>

#include "choices.hpp"
#include "icon-theme-names.hpp"
#include "pane-parts.hpp"

static const QString gtk_table = QStringLiteral("gtk");
static const QString qt_table = QStringLiteral("qt");
static const QString icons_table = QStringLiteral("icons");
// [icons]: the icon theme for programs, and the one for the Dock, the menu
// bar and the rest of the shell. Either left out: the theme's own.
static const QStringList icon_theme_keys = {QStringLiteral("programs"), QStringLiteral("dock_and_menu_bar")};

// Switching to the running desktop rewrites the icon themes for GTK, Qt and
// KDE programs, tells services about them and restarts the Dock and menu bar
// (src/session/bin/mavernix-switch).
static const QString switch_command = QStringLiteral("mavernix-switch");

// How long an icon theme box must rest before its choice is applied.
static const int icon_theme_rest_ms = 700;

bool apply_icon_theme(const Choices& choices)
{
    return QProcess::startDetached(switch_command, {choices.runningDesktop()});
}

QStringList icon_theme_choices(const Choices& choices)
{
    QStringList picked;
    for (const QString& key : icon_theme_keys)
    {
        picked.append(choices.current(icons_table, key).toString());
    }
    return picked;
}

// A checkbox for a toolkit's "use_theme" key.
static QCheckBox* use_theme_box(Choices& choices, const QString& table, const QString& words)
{
    auto* box = new QCheckBox(words);
    box->setChecked(choices.value(table, QStringLiteral("use_theme")).toBool());
    QObject::connect(box, &QCheckBox::toggled, box,
        [&choices, table](bool on) { choices.set(table, QStringLiteral("use_theme"), on); });
    return box;
}

// What the icon theme boxes say, from the config's words.
struct IconThemeWords
{
    QString own;
    QString used_by_plasma;
    QString used_by_gnome;
};

// One entry of an icon theme box: what it says, and the folder name saved
// (empty for the theme's own).
struct IconThemeItem
{
    QString text;
    QString folder;
};

// The theme's own first, then every installed icon theme. Those that Plasma
// or GNOME use say so, for people coming from there who know their theme by
// its look, not its name.
static QList<IconThemeItem> icon_theme_items(const Choices& choices, const IconThemeWords& words)
{
    const QString own = choices.themeIconTheme();
    QList<IconThemeItem> items = {{own.isEmpty() ? words.own : QStringLiteral("%1 (%2)").arg(words.own, own), {}}};
    const QString plasma = plasma_icon_theme();
    const QString gnome = gnome_icon_theme();
    for (const InstalledIconTheme& theme : installed_icon_themes())
    {
        // Plasma copies its icon theme into GNOME's settings for GTK
        // programs, so the same name in both is Plasma's.
        const QString mark = theme.folder == plasma  ? words.used_by_plasma
                             : theme.folder == gnome ? words.used_by_gnome
                                                     : QString();
        items.append({mark.isEmpty() ? theme.name : QStringLiteral("%1 — %2").arg(theme.name, mark), theme.folder});
    }
    return items;
}

// A box for one of the [icons] keys.
static QComboBox* icon_theme_box(Choices& choices, const QList<IconThemeItem>& items, const QString& key)
{
    auto* box = new QComboBox;
    for (const IconThemeItem& item : items)
    {
        box->addItem(item.text, item.folder);
    }
    // The line between the theme's own and the installed ones.
    box->insertSeparator(1);

    // A picked theme that is gone since is still shown, so the box does not
    // claim the theme's own; the session uses a stand-in for it.
    const QString picked = choices.current(icons_table, key).toString();
    if (!picked.isEmpty() && box->findData(picked) < 0)
    {
        box->addItem(picked, picked);
    }
    box->setCurrentIndex(std::max(0, box->findData(picked)));

    // Arrow keys and the mouse wheel step through a closed box, each step a
    // pick: the choice is written at once, but applied, which restarts the
    // Dock and menu bar, only once the box rests. One still waiting when the
    // window closes is applied then.
    auto* rest = new QTimer(box);
    rest->setSingleShot(true);
    rest->setInterval(icon_theme_rest_ms);
    QObject::connect(rest, &QTimer::timeout, box, [&choices]() { apply_icon_theme(choices); });
    QObject::connect(qApp, &QCoreApplication::aboutToQuit, rest, [&choices, rest]() {
        if (rest->isActive())
        {
            rest->stop();
            apply_icon_theme(choices);
        }
    });

    QObject::connect(box, &QComboBox::activated, box, [&choices, box, key, rest](int index) {
        const QString folder = box->itemData(index).toString();
        if (folder == choices.current(icons_table, key).toString())
        {
            return;
        }
        if (folder.isEmpty())
        {
            choices.remove({{icons_table, key}});
        }
        else
        {
            choices.set(icons_table, key, folder);
        }
        rest->start();
    });
    return box;
}

ToolkitsPane::ToolkitsPane(Choices& choices, QWidget* parent) : QWidget(parent), choices(choices)
{
    const QVariantMap words = choices.configTable(QStringLiteral("settings")).value(QStringLiteral("words")).toMap();
    const auto word = [&words](const char* key, const char* fallback) {
        return words.value(QLatin1String(key), QString::fromUtf8(fallback)).toString();
    };

    const QList<IconThemeItem> icon_themes = icon_theme_items(
        choices, {word("toolkits_icons_own", "The theme's own"), word("toolkits_icons_plasma", "used by Plasma"),
                     word("toolkits_icons_gnome", "used by GNOME")});
    auto* program_icons = icon_theme_box(choices, icon_themes, icon_theme_keys[0]);
    auto* desktop_icons = icon_theme_box(choices, icon_themes, icon_theme_keys[1]);

    auto* gtk_theme = use_theme_box(choices, gtk_table, word("toolkits_gtk_theme", "Drawn in the desktop's look"));

    auto* qt_theme = use_theme_box(choices, qt_table, word("toolkits_qt_theme", "Drawn in the desktop's look"));

    // Labels in one column, ending where the controls begin.
    auto* form = new QFormLayout(this);
    form->setLabelAlignment(Qt::AlignRight | Qt::AlignTop);
    form->setFormAlignment(Qt::AlignHCenter | Qt::AlignTop);
    form->setVerticalSpacing(14);
    // A control is as wide as what it shows, not as the window.
    form->setFieldGrowthPolicy(QFormLayout::FieldsStayAtSizeHint);
    form->addRow(word("toolkits_icons", "Icons in programs:"),
        with_note_under(program_icons,
            word("toolkits_icons_note",
                "GTK, Qt and KDE programs. Programs that are open keep theirs until they start again."),
            0));
    form->addRow(word("toolkits_icons_desktop", "Icons in the Dock\nand menu bar:"),
        with_note_under(desktop_icons,
            word("toolkits_icons_desktop_note", "Also the app grid, the program switcher and the search."), 0));
    form->addRow(word("toolkits_gtk", "GTK programs:"), with_note(gtk_theme, word("toolkits_gtk_theme_note", "")));
    form->addRow(word("toolkits_qt", "Qt programs:"), with_note(qt_theme, word("toolkits_qt_theme_note", "")));
}
