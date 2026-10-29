#include "desktops-pane.hpp"

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QSpinBox>

#include "choices.hpp"

// How long a window is held at the screen's side before it goes to the
// next desktop, when the config names no time.
static const int usual_edge_wait_ms = 900;
// The compositor's word for "never".
static const int never = -1;

DesktopsPane::DesktopsPane(Choices& choices, QWidget* parent) : QWidget(parent), choices(choices)
{
    const QVariantMap words = choices.configTable(QStringLiteral("settings")).value(QStringLiteral("words")).toMap();
    const auto word = [&words](const char* key, const char* fallback) {
        return words.value(QLatin1String(key), QString::fromUtf8(fallback)).toString();
    };
    const QString table = QStringLiteral("desktops");

    // How many desktops. (The overview of all windows changes the same
    // number; whichever was used last holds.)
    auto* count = new QSpinBox;
    const int most = choices.value(table, QStringLiteral("most")).toInt();
    count->setRange(1, most > 0 ? most : 16);
    count->setValue(qMax(1, choices.value(table, QStringLiteral("count")).toInt()));
    connect(count, &QSpinBox::valueChanged, this,
        [this, table](int value) { this->choices.set(table, QStringLiteral("count"), value); });

    auto* swipe = new QComboBox;
    const struct
    {
        int fingers;
        const char* word_key;
        const char* fallback;
    } swipe_choices[] = {
        {0, "desktops_swipe_off", "Off"},
        {3, "desktops_swipe_three", "Three fingers"},
        {4, "desktops_swipe_four", "Four fingers"},
    };
    const int chosen_fingers = choices.value(table, QStringLiteral("swipe_fingers")).toInt();
    for (const auto& choice : swipe_choices)
    {
        swipe->addItem(word(choice.word_key, choice.fallback), choice.fingers);
        if (chosen_fingers == choice.fingers)
        {
            swipe->setCurrentIndex(swipe->count() - 1);
        }
    }
    connect(swipe, &QComboBox::activated, this, [this, table, swipe](int index) {
        this->choices.set(table, QStringLiteral("swipe_fingers"), swipe->itemData(index).toInt());
    });

    // Whether a window held at the screen's side goes to the next desktop.
    // Switched on again it waits as long as it did before, or the usual time.
    const QVariant wait_now = choices.value(table, QStringLiteral("drag_to_edge_ms"));
    const int wait_when_on = wait_now.toInt() > 0 ? wait_now.toInt() : usual_edge_wait_ms;
    auto* edge =
        new QCheckBox(word("desktops_drag_to_edge", "A window held at the screen's side goes to the next desktop"));
    edge->setChecked(wait_now.isValid() && wait_now.toInt() > 0);
    connect(edge, &QCheckBox::toggled, this, [this, table, wait_when_on](bool on) {
        this->choices.set(table, QStringLiteral("drag_to_edge_ms"), on ? wait_when_on : never);
    });

    // Labels in one column, ending where the controls begin.
    auto* form = new QFormLayout(this);
    form->setLabelAlignment(Qt::AlignRight | Qt::AlignTop);
    form->setFormAlignment(Qt::AlignHCenter | Qt::AlignTop);
    form->setVerticalSpacing(14);
    // A control is as wide as what it shows, not as the window.
    form->setFieldGrowthPolicy(QFormLayout::FieldsStayAtSizeHint);
    form->addRow(word("desktops_count", "Desktops:"), count);
    form->addRow(word("desktops_swipe", "Swipe between desktops:"), swipe);
    form->addRow(QString(), edge);
}
