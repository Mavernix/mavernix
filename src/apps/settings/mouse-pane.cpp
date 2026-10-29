#include "mouse-pane.hpp"

#include <QButtonGroup>
#include <QCheckBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QProcess>
#include <QRadioButton>
#include <QSlider>
#include <QTimer>
#include <QVBoxLayout>

#include <cmath>
#include <iterator>

#include "choices.hpp"
#include "pane-parts.hpp"

// Rewrites the compositor's settings from the config and the user's choices
// (on the path from src/session/start-desktop); the compositor follows at once.
static const QString reconfigure_command = QStringLiteral("mavernix-reconfigure");

// The compositor's pointer speed runs from -1 to 1; the slider has this many
// stops to each side of the middle.
static const int speed_steps = 5;
// The wheel's speed is a factor; 1 is unchanged.
static const double scroll_stops[] = {0.25, 0.5, 0.75, 1.0, 1.5, 2.0, 2.5, 3.0};
static const int scroll_stop_count = int(std::size(scroll_stops));
static const int slider_width_px = 200;
// Each write rewrites the compositor's config, so holding an arrow key on
// a slider writes once it is let go.
static const int write_delay_ms = 500;

static int nearest_scroll_stop(double factor)
{
    int nearest = 0;
    for (int stop = 1; stop < scroll_stop_count; stop++)
    {
        if (std::abs(scroll_stops[stop] - factor) < std::abs(scroll_stops[nearest] - factor))
        {
            nearest = stop;
        }
    }
    return nearest;
}

MousePane::MousePane(Choices& choices, QWidget* parent) : QWidget(parent), choices(choices)
{
    const QVariantMap words = choices.configTable(QStringLiteral("settings")).value(QStringLiteral("words")).toMap();
    const auto word = [&words](const char* key, const char* fallback) {
        return words.value(QLatin1String(key), QString::fromUtf8(fallback)).toString();
    };
    slow_word = word("mouse_speed_slow", "Slow");
    fast_word = word("mouse_speed_fast", "Fast");
    const QString table = QStringLiteral("input");

    write_later = new QTimer(this);
    write_later->setSingleShot(true);
    write_later->setInterval(write_delay_ms);
    connect(write_later, &QTimer::timeout, this, &MousePane::writeSliders);

    auto* natural = new QCheckBox(word("mouse_natural_scroll", "Natural scrolling"));
    natural->setChecked(choices.value(table, QStringLiteral("mouse_natural_scroll")).toBool());
    connect(natural, &QCheckBox::toggled, this,
        [this](bool on) { change(QStringLiteral("mouse_natural_scroll"), on); });

    const QVariant chosen_scroll = choices.value(table, QStringLiteral("mouse_scroll_speed"));
    auto* speed_row = speedSlider(speed, 2 * speed_steps + 1,
        speed_steps + qRound(choices.value(table, QStringLiteral("mouse_speed")).toDouble() * speed_steps));
    auto* scrolling_row = speedSlider(scrolling, scroll_stop_count,
        nearest_scroll_stop(chosen_scroll.isValid() ? chosen_scroll.toDouble() : 1.0));

    // Left out, the device's own profile applies, which for a mouse is
    // acceleration.
    const QVariant accelerated = choices.value(table, QStringLiteral("mouse_acceleration"));
    auto* acceleration = new QCheckBox(word("mouse_acceleration", "Acceleration"));
    acceleration->setChecked(!accelerated.isValid() || accelerated.toBool());
    connect(acceleration, &QCheckBox::toggled, this,
        [this](bool on) { change(QStringLiteral("mouse_acceleration"), on); });

    auto* left = new QRadioButton(word("mouse_primary_left", "Left"));
    auto* right = new QRadioButton(word("mouse_primary_right", "Right"));
    auto* buttons = new QButtonGroup(this);
    buttons->addButton(left);
    buttons->addButton(right);
    (choices.value(table, QStringLiteral("left_handed")).toBool() ? right : left)->setChecked(true);
    connect(right, &QRadioButton::toggled, this, [this](bool on) { change(QStringLiteral("left_handed"), on); });
    auto* primary = new QWidget;
    auto* primary_rows = new QVBoxLayout(primary);
    primary_rows->setContentsMargins(0, 0, 0, 0);
    primary_rows->setSpacing(4);
    primary_rows->addWidget(left);
    primary_rows->addWidget(right);

    // As on the original: the scroll direction across the top, the rest below.
    auto* rows = new QVBoxLayout(this);
    rows->setSpacing(pane_spacing_px);
    rows->addWidget(with_note(natural, word("mouse_natural_scroll_note", "")), 0, Qt::AlignHCenter);

    auto* form = new QFormLayout;
    form->setLabelAlignment(Qt::AlignRight | Qt::AlignTop);
    form->setFormAlignment(Qt::AlignHCenter | Qt::AlignTop);
    form->setVerticalSpacing(pane_spacing_px);
    form->addRow(word("mouse_speed", "Tracking speed:"), speed_row);
    form->addRow(word("mouse_scroll_speed", "Scrolling speed:"), scrolling_row);
    form->addRow(QString(), with_note(acceleration, word("mouse_acceleration_note", "")));
    form->addRow(word("mouse_primary", "Primary mouse button:"), primary);
    rows->addLayout(form);
    rows->addStretch();
}

QWidget* MousePane::speedSlider(QSlider*& slider, int stops, int at)
{
    slider = new QSlider(Qt::Horizontal);
    slider->setRange(0, stops - 1);
    slider->setPageStep(1);
    slider->setTickPosition(QSlider::TicksBelow);
    slider->setTickInterval(1);
    slider->setFixedWidth(slider_width_px);
    slider->setValue(at);
    // Dragged: written on release. Moved with the keys: once they rest.
    QSlider* const moved = slider;
    connect(slider, &QSlider::valueChanged, this, [this, moved]() {
        if (!moved->isSliderDown())
        {
            write_later->start();
        }
    });
    connect(slider, &QSlider::sliderReleased, this, &MousePane::writeSliders);
    return with_end_words(slider, slow_word, fast_word);
}

void MousePane::writeSliders()
{
    write_later->stop();
    choices.set(QStringLiteral("input"), QStringLiteral("mouse_speed"), double(speed->value() - speed_steps) / speed_steps);
    change(QStringLiteral("mouse_scroll_speed"), scroll_stops[scrolling->value()]);
}

void MousePane::change(const QString& key, const QVariant& value)
{
    choices.set(QStringLiteral("input"), key, value);
    QProcess::startDetached(reconfigure_command, {});
}
