#include "windows-pane.hpp"

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QProcess>

#include "choices.hpp"
#include "pane-parts.hpp"

// Rewrites the settings GTK programs follow (src/session/start-desktop puts
// it on the path); window frames follow the user's file by themselves.
static const QString reconfigure_command = QStringLiteral("mavernix-reconfigure");
static const QString left_side = QStringLiteral("left");
static const QString close_button = QStringLiteral("close");

// The buttons from left to right, with the close button moved to the
// window's outer end on the chosen side and the others in their order.
static QVariantList order_for_side(const QVariant& order, const QString& side)
{
    QVariantList arranged;
    for (const QVariant& button : order.toList())
    {
        if (button.toString() != close_button)
        {
            arranged.append(button);
        }
    }
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

WindowsPane::WindowsPane(Choices& choices, QWidget* parent) : QWidget(parent), choices(choices)
{
    const QVariantMap words = choices.configTable(QStringLiteral("settings")).value(QStringLiteral("words")).toMap();
    const auto word = [&words](const char* key, const char* fallback) {
        return words.value(QLatin1String(key), QLatin1String(fallback)).toString();
    };
    const QString table = QStringLiteral("window");

    auto* snap = new QCheckBox(word("windows_snap", "Windows snap to the screen's sides"));
    snap->setChecked(choices.value(table, QStringLiteral("snap_to_edges")).toBool());
    connect(snap, &QCheckBox::toggled, this,
        [this, table](bool on) { this->choices.set(table, QStringLiteral("snap_to_edges"), on); });

    // The key held to move a window by dragging anywhere on it, and to
    // resize it with the secondary button; written as the compositor
    // writes keys.
    auto* drag_key = new QComboBox;
    const struct
    {
        const char* key;
        const char* word_key;
        const char* fallback;
    } key_choices[] = {
        {"", "windows_drag_key_none", "No key"},
        {"<super>", "windows_drag_key_super", "Super"},
        {"<alt>", "windows_drag_key_alt", "Alt"},
    };
    const QString chosen_key = choices.value(table, QStringLiteral("drag_key")).toString();
    for (const auto& choice : key_choices)
    {
        drag_key->addItem(word(choice.word_key, choice.fallback), QLatin1String(choice.key));
        if (chosen_key == QLatin1String(choice.key))
        {
            drag_key->setCurrentIndex(drag_key->count() - 1);
        }
    }
    connect(drag_key, &QComboBox::activated, this, [this, table, drag_key](int index) {
        this->choices.set(table, QStringLiteral("drag_key"), drag_key->itemData(index).toString());
    });

    // A double click on a title bar ([window] title_double_click): a choice
    // rather than a minimise tick box, as other desktops enlarge the window.
    auto* double_click = new QComboBox;
    const struct
    {
        const char* value;
        const char* word_key;
        const char* fallback;
    } double_click_choices[] = {
        {"none", "window_double_click_none", "does nothing"},
        {"minimize", "window_double_click_minimize", "minimises the window"},
        {"maximize", "window_double_click_maximize", "enlarges the window"},
    };
    const QString chosen_double_click = choices.value(table, QStringLiteral("title_double_click")).toString();
    for (const auto& choice : double_click_choices)
    {
        double_click->addItem(word(choice.word_key, choice.fallback), QLatin1String(choice.value));
        if (chosen_double_click == QLatin1String(choice.value))
        {
            double_click->setCurrentIndex(double_click->count() - 1);
        }
    }
    connect(double_click, &QComboBox::activated, this, [this, table, double_click](int index) {
        this->choices.set(table, QStringLiteral("title_double_click"), double_click->itemData(index).toString());
    });

    // Which side the close, minimise and enlarge buttons are on: for the
    // window frames and for programs that draw their own title bar alike.
    auto* button_side = new QComboBox;
    button_side->addItem(word("windows_buttons_left", "At the left"), left_side);
    button_side->addItem(word("windows_buttons_right", "At the right"), QStringLiteral("right"));
    button_side->setCurrentIndex(qMax(0, button_side->findData(choices.value(table, QStringLiteral("button_side")).toString())));
    connect(button_side, &QComboBox::activated, this, [this, table, button_side](int index) {
        const QString side = button_side->itemData(index).toString();
        const QVariantList order = order_for_side(this->choices.value(table, QStringLiteral("button_order")), side);
        for (const QString& toolkit_table : {table, QStringLiteral("gtk")})
        {
            this->choices.set(toolkit_table, QStringLiteral("button_side"), side);
            this->choices.set(toolkit_table, QStringLiteral("button_order"), order);
        }
        QProcess::startDetached(reconfigure_command, {});
    });

    // Labels in one column, ending where the controls begin.
    auto* form = new QFormLayout(this);
    form->setLabelAlignment(Qt::AlignRight | Qt::AlignTop);
    form->setFormAlignment(Qt::AlignHCenter | Qt::AlignTop);
    form->setVerticalSpacing(14);
    // A control is as wide as what it shows, not as the window.
    form->setFieldGrowthPolicy(QFormLayout::FieldsStayAtSizeHint);
    form->addRow(QString(),
        with_note(snap,
            word("windows_snap_note",
                "At the left or right side: that half. In a corner: that quarter. At the top: the whole screen.")));
    form->addRow(word("windows_drag_key", "Move and resize a window by holding:"), drag_key);
    form->addRow(word("window_double_click", "A double click on a title bar:"), double_click);
    form->addRow(word("windows_buttons", "Window buttons:"), button_side);
}
