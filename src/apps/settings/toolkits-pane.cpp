#include "toolkits-pane.hpp"

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QProcess>

#include <algorithm>

#include "choices.hpp"
#include "icon-theme-names.hpp"
#include "pane-parts.hpp"

static const QString gtk_table = QStringLiteral("gtk");
static const QString qt_table = QStringLiteral("qt");
static const QString icons_table = QStringLiteral("icons");
static const QString icon_theme_key = QStringLiteral("theme");

// Switching to the running desktop rewrites the icon theme for GTK, Qt and
// KDE programs, tells services about it and restarts the Dock and menu bar
// (src/session/bin/mavernix-switch).
static const QString switch_command = QStringLiteral("mavernix-switch");

bool apply_icon_theme(const Choices& choices)
{
    return QProcess::startDetached(switch_command, {choices.runningDesktop()});
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

// The icon theme: the theme's own first (no key in the user's file), then
// every installed one.
static QComboBox* icon_theme_box(Choices& choices, const QString& own_words)
{
    auto* box = new QComboBox;
    const QString own = choices.themeIconTheme();
    box->addItem(own.isEmpty() ? own_words : QStringLiteral("%1 (%2)").arg(own_words, own), QString());
    box->insertSeparator(1);
    for (const InstalledIconTheme& theme : installed_icon_themes())
    {
        box->addItem(theme.name, theme.folder);
    }

    // A picked theme that is gone since is still shown, so the box does not
    // claim the theme's own; the session uses a stand-in for it.
    const QString picked = choices.current(icons_table, icon_theme_key).toString();
    if (!picked.isEmpty() && box->findData(picked) < 0)
    {
        box->addItem(picked, picked);
    }
    box->setCurrentIndex(std::max(0, box->findData(picked)));

    QObject::connect(box, &QComboBox::activated, box, [&choices, box](int index) {
        const QString folder = box->itemData(index).toString();
        if (folder == choices.current(icons_table, icon_theme_key).toString())
        {
            return;
        }
        if (folder.isEmpty())
        {
            choices.remove({{icons_table, icon_theme_key}});
        }
        else
        {
            choices.set(icons_table, icon_theme_key, folder);
        }
        apply_icon_theme(choices);
    });
    return box;
}

ToolkitsPane::ToolkitsPane(Choices& choices, QWidget* parent) : QWidget(parent), choices(choices)
{
    const QVariantMap words = choices.configTable(QStringLiteral("settings")).value(QStringLiteral("words")).toMap();
    const auto word = [&words](const char* key, const char* fallback) {
        return words.value(QLatin1String(key), QString::fromUtf8(fallback)).toString();
    };

    auto* icon_theme = icon_theme_box(choices, word("toolkits_icons_own", "The theme's own"));

    auto* gtk_theme = use_theme_box(choices, gtk_table, word("toolkits_gtk_theme", "Drawn in the desktop's look"));

    auto* qt_theme = use_theme_box(choices, qt_table, word("toolkits_qt_theme", "Drawn in the desktop's look"));

    // Labels in one column, ending where the controls begin.
    auto* form = new QFormLayout(this);
    form->setLabelAlignment(Qt::AlignRight | Qt::AlignTop);
    form->setFormAlignment(Qt::AlignHCenter | Qt::AlignTop);
    form->setVerticalSpacing(14);
    // A control is as wide as what it shows, not as the window.
    form->setFieldGrowthPolicy(QFormLayout::FieldsStayAtSizeHint);
    form->addRow(word("toolkits_icons", "Icons:"),
        with_note_under(icon_theme,
            word("toolkits_icons_note", "The icons in programs, the Dock and the menu bar. Programs that are open "
                                        "keep theirs until they start again."),
            0));
    form->addRow(word("toolkits_gtk", "GTK programs:"), with_note(gtk_theme, word("toolkits_gtk_theme_note", "")));
    form->addRow(word("toolkits_qt", "Qt programs:"), with_note(qt_theme, word("toolkits_qt_theme_note", "")));
}
