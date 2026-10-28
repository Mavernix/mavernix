#include "mouse-pane.hpp"

#include <QCheckBox>
#include <QFormLayout>
#include <QProcess>
#include <QSlider>
#include <QTimer>

#include "choices.hpp"
#include "pane-parts.hpp"

// Rewrites the compositor's settings from the config and the user's choices
// (on the path from src/session/start-desktop); the compositor follows at once.
static const QString reconfigure_command = QStringLiteral("mavernix-reconfigure");

// The compositor's speed runs from -1 to 1; the slider has this many stops
// to each side of the middle.
static const int speed_steps = 10;
static const int slider_width_px = 260;
// Each write rewrites the compositor's config, so holding an arrow key on
// the slider writes once it is let go.
static const int write_delay_ms = 500;

MousePane::MousePane(Choices& choices, QWidget* parent) : QWidget(parent), choices(choices)
{
    const QVariantMap words = choices.configTable(QStringLiteral("settings")).value(QStringLiteral("words")).toMap();
    const auto word = [&words](const char* key, const char* fallback) {
        return words.value(QLatin1String(key), QLatin1String(fallback)).toString();
    };
    const QString table = QStringLiteral("input");

    write_later = new QTimer(this);
    write_later->setSingleShot(true);
    write_later->setInterval(write_delay_ms);
    connect(write_later, &QTimer::timeout, this, &MousePane::writeSpeed);

    speed = new QSlider(Qt::Horizontal);
    speed->setRange(-speed_steps, speed_steps);
    speed->setPageStep(1);
    speed->setTickPosition(QSlider::TicksBelow);
    speed->setTickInterval(speed_steps / 2);
    speed->setFixedWidth(slider_width_px);
    speed->setValue(qRound(choices.value(table, QStringLiteral("mouse_speed")).toDouble() * speed_steps));
    // Dragged: written on release. Moved with the keys: once they rest.
    connect(speed, &QSlider::valueChanged, this, [this]() {
        if (!speed->isSliderDown())
        {
            write_later->start();
        }
    });
    connect(speed, &QSlider::sliderReleased, this, &MousePane::writeSpeed);

    // Left out, the device's own profile applies, which for a mouse is
    // acceleration.
    const QVariant accelerated = choices.value(table, QStringLiteral("mouse_acceleration"));
    auto* acceleration = new QCheckBox(word("mouse_acceleration", "Acceleration"));
    acceleration->setChecked(!accelerated.isValid() || accelerated.toBool());
    connect(acceleration, &QCheckBox::toggled, this,
        [this](bool on) { change(QStringLiteral("mouse_acceleration"), on); });

    auto* natural = new QCheckBox(word("mouse_natural_scroll", "Natural scrolling"));
    natural->setChecked(choices.value(table, QStringLiteral("mouse_natural_scroll")).toBool());
    connect(natural, &QCheckBox::toggled, this,
        [this](bool on) { change(QStringLiteral("mouse_natural_scroll"), on); });

    auto* form = new QFormLayout(this);
    form->setLabelAlignment(Qt::AlignRight | Qt::AlignTop);
    form->setFormAlignment(Qt::AlignHCenter | Qt::AlignTop);
    form->setVerticalSpacing(pane_spacing_px);
    form->addRow(word("mouse_speed", "Tracking speed:"),
        with_end_words(speed, word("mouse_speed_slow", "Slow"), word("mouse_speed_fast", "Fast")));
    form->addRow(QString(), with_note(acceleration, word("mouse_acceleration_note", "")));
    form->addRow(QString(), with_note(natural, word("mouse_natural_scroll_note", "")));
}

void MousePane::writeSpeed()
{
    write_later->stop();
    change(QStringLiteral("mouse_speed"), double(speed->value()) / speed_steps);
}

void MousePane::change(const QString& key, const QVariant& value)
{
    choices.set(QStringLiteral("input"), key, value);
    QProcess::startDetached(reconfigure_command, {});
}
