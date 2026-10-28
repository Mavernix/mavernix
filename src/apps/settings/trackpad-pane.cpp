#include "trackpad-pane.hpp"

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QSlider>
#include <QTimer>
#include <QProcess>
#include <QVBoxLayout>

#include "choices.hpp"
#include "pane-parts.hpp"

// Rewrites the compositor's settings from the config and the user's choices
// (on the path from src/session/start-desktop); the compositor follows at once.
static const QString reconfigure_command = QStringLiteral("mavernix-reconfigure");

TrackpadPane::TrackpadPane(Choices& choices, QWidget* parent) : QWidget(parent), choices(choices)
{
    const QVariantMap words = choices.configTable(QStringLiteral("settings")).value(QStringLiteral("words")).toMap();
    const auto word = [&words](const char* key, const char* fallback) {
        return words.value(QLatin1String(key), QLatin1String(fallback)).toString();
    };

    auto* tap = new QCheckBox(word("trackpad_tap", "Tap to click"));
    tap->setChecked(choices.value(QStringLiteral("input"), QStringLiteral("tap_to_click")).toBool());
    connect(tap, &QCheckBox::toggled, this, [this](bool on) { change(QStringLiteral("tap_to_click"), on); });

    auto* natural = new QCheckBox(word("trackpad_natural_scroll", "Natural scrolling"));
    natural->setChecked(choices.value(QStringLiteral("input"), QStringLiteral("natural_scroll")).toBool());
    connect(natural, &QCheckBox::toggled, this, [this](bool on) { change(QStringLiteral("natural_scroll"), on); });

    // How fast the pointer follows the fingers: the compositor's -1 to 1 in
    // tenths, written once the slider is let go or the keys rest.
    auto* speed = new QSlider(Qt::Horizontal);
    speed->setRange(-5, 5);
    speed->setPageStep(1);
    speed->setTickPosition(QSlider::TicksBelow);
    speed->setTickInterval(1);
    speed->setFixedWidth(200);
    speed->setValue(qRound(choices.value(QStringLiteral("input"), QStringLiteral("touchpad_speed")).toDouble() * 5));
    auto* write_later = new QTimer(this);
    write_later->setSingleShot(true);
    write_later->setInterval(500);
    const auto write_speed = [this, speed, write_later]() {
        write_later->stop();
        this->choices.set(QStringLiteral("input"), QStringLiteral("touchpad_speed"), speed->value() / 5.0);
        QProcess::startDetached(reconfigure_command, {});
    };
    connect(write_later, &QTimer::timeout, this, write_speed);
    connect(speed, &QSlider::sliderReleased, this, write_speed);
    connect(speed, &QSlider::valueChanged, this, [speed, write_later]() {
        if (!speed->isSliderDown())
        {
            write_later->start();
        }
    });

    const QVariant accelerated = choices.value(QStringLiteral("input"), QStringLiteral("touchpad_acceleration"));
    auto* acceleration = new QCheckBox(word("trackpad_acceleration", "Acceleration"));
    acceleration->setChecked(!accelerated.isValid() || accelerated.toBool());
    connect(acceleration, &QCheckBox::toggled, this,
        [this](bool on) { change(QStringLiteral("touchpad_acceleration"), on); });

    // A secondary ("right") click: two fingers anywhere, or the lower right corner.
    auto* secondary = new QComboBox;
    secondary->addItem(word("trackpad_secondary_two_fingers", "Click with two fingers"), QStringLiteral("two_fingers"));
    secondary->addItem(word("trackpad_secondary_corner", "Click in the lower right corner"), QStringLiteral("corner"));
    secondary->setCurrentIndex(qMax(0, secondary->findData(choices.value(QStringLiteral("input"), QStringLiteral("secondary_click")).toString())));
    connect(secondary, &QComboBox::activated, this, [this, secondary](int index) {
        this->choices.set(QStringLiteral("input"), QStringLiteral("secondary_click"), secondary->itemData(index));
        QProcess::startDetached(reconfigure_command, {});
    });

    auto* form = new QFormLayout(this);
    form->setLabelAlignment(Qt::AlignRight | Qt::AlignTop);
    form->setFormAlignment(Qt::AlignHCenter | Qt::AlignTop);
    form->setVerticalSpacing(14);
    form->addRow(QString(), with_note(tap, word("trackpad_tap_note", "")));
    form->addRow(QString(), with_note(natural, word("trackpad_natural_scroll_note", "")));
    form->addRow(word("trackpad_speed", "Tracking speed:"),
        with_end_words(speed, word("mouse_speed_slow", "Slow"), word("mouse_speed_fast", "Fast")));
    form->addRow(QString(), acceleration);
    form->addRow(word("trackpad_secondary", "Secondary click:"), secondary);
}

void TrackpadPane::change(const QString& key, bool on)
{
    choices.set(QStringLiteral("input"), key, on);
    QProcess::startDetached(reconfigure_command, {});
}
