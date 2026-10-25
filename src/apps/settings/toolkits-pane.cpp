#include "toolkits-pane.hpp"

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QProcess>

#include "choices.hpp"
#include "pane-parts.hpp"

// The session's command that writes the settings GTK programs follow
// anew (src/session/start-desktop puts it on the path). GTK programs take a
// changed layout up while they run.
static const QString reconfigure_command = QStringLiteral("mavernix-reconfigure");

static const QString gtk_table = QStringLiteral("gtk");
static const QString qt_table = QStringLiteral("qt");
static const QString left_side = QStringLiteral("left");
static const QString right_side = QStringLiteral("right");
static const QString close_button = QStringLiteral("close");

// The buttons in the order they are shown from left to right, with the
// close button moved to the window's outer end on the chosen side and the
// others left in their order: first at the left, last at the right. One
// rule gives the usual order at either side, whatever order the config
// started from, so no order is named in code.
static QVariantList order_for_side(const QVariant& order, const QString& side)
{
    QVariantList others;
    for (const QVariant& button : order.toList())
    {
        if (button.toString() != close_button)
        {
            others.append(button);
        }
    }

    QVariantList arranged = others;
    if (side == left_side)
    {
        arranged.prepend(close_button);
    }
    else
    {
        arranged.append(close_button);
    }

    return arranged;
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

ToolkitsPane::ToolkitsPane(Choices& choices, QWidget* parent) : QWidget(parent), choices(choices)
{
    const QVariantMap words = choices.configTable(QStringLiteral("settings")).value(QStringLiteral("words")).toMap();
    const auto word = [&words](const char* key, const char* fallback) {
        return words.value(QLatin1String(key), QLatin1String(fallback)).toString();
    };

    auto* gtk_theme = use_theme_box(choices, gtk_table, word("toolkits_gtk_theme", "Drawn in the desktop's look"));

    // Where the window buttons are: the [gtk] table says, and where it does
    // not the window frames' own side does (the same fallback the session
    // uses, src/session/write_interface_settings.py).
    QVariant side = choices.value(gtk_table, QStringLiteral("button_side"));
    if (!side.isValid())
    {
        side = choices.value(QStringLiteral("window"), QStringLiteral("button_side"));
    }
    auto* button_side = new QComboBox;
    button_side->addItem(word("toolkits_buttons_right", "At the right"), right_side);
    button_side->addItem(word("toolkits_buttons_left", "At the left"), left_side);
    button_side->setCurrentIndex(button_side->findData(side.toString()));
    connect(button_side, &QComboBox::activated, this,
        [this, button_side](int index) { changeButtonSide(button_side->itemData(index).toString()); });

    auto* qt_theme = use_theme_box(choices, qt_table, word("toolkits_qt_theme", "Drawn in the desktop's look"));

    // Labels in one column, ending where the controls begin.
    auto* form = new QFormLayout(this);
    form->setLabelAlignment(Qt::AlignRight | Qt::AlignTop);
    form->setFormAlignment(Qt::AlignHCenter | Qt::AlignTop);
    form->setVerticalSpacing(14);
    // A control is as wide as what it shows, not as the window.
    form->setFieldGrowthPolicy(QFormLayout::FieldsStayAtSizeHint);
    form->addRow(word("toolkits_gtk", "GTK programs:"), with_note(gtk_theme, word("toolkits_gtk_theme_note", "")));
    form->addRow(word("toolkits_buttons", "Window buttons:"),
        with_note_under(button_side, word("toolkits_buttons_note", ""), 0));
    form->addRow(word("toolkits_qt", "Qt programs:"), with_note(qt_theme, word("toolkits_qt_theme_note", "")));
}

void ToolkitsPane::changeButtonSide(const QString& side)
{
    // The order is the [gtk] table's own, or the window frames' where the
    // table has none.
    QVariant order = choices.value(gtk_table, QStringLiteral("button_order"));
    if (!order.isValid())
    {
        order = choices.value(QStringLiteral("window"), QStringLiteral("button_order"));
    }

    choices.set(gtk_table, QStringLiteral("button_side"), side);
    choices.set(gtk_table, QStringLiteral("button_order"), order_for_side(order, side));
    QProcess::startDetached(reconfigure_command, {});
}
