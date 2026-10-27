#include "energy-pane.hpp"

#include <QCheckBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QSlider>

#include "choices.hpp"

// In minutes; "never" comes after them.
static const int stop_minutes[] = {1, 3, 5, 10, 15, 20, 25, 30, 45, 60, 75, 90, 105, 120, 180};
static const int seconds_per_minute = 60;
static const int minutes_per_hour = 60;
// Wide enough that neighbouring stops are easy to tell apart.
static const int slider_width_px = 320;

EnergyPane::EnergyPane(Choices& choices, QWidget* parent) : QWidget(parent), choices(choices)
{
    words = choices.configTable(QStringLiteral("settings")).value(QStringLiteral("words")).toMap();
    const auto word = [this](const char* key, const char* fallback) {
        return words.value(QLatin1String(key), QLatin1String(fallback)).toString();
    };

    for (const int minutes : stop_minutes)
    {
        stops.append(minutes * seconds_per_minute);
    }
    stops.append(0);

    auto* screen_off_row = timeSlider(QStringLiteral("screen_off_after_seconds"), screen_off);
    auto* sleep_row = timeSlider(QStringLiteral("sleep_after_seconds"), sleep);

    locks = new QCheckBox(word("energy_lock", "Lock the screen when it switches off and before sleep"));
    locks->setChecked(choices.value(QStringLiteral("idle"), QStringLiteral("lock_before_sleep")).toBool() ||
                      choices.value(QStringLiteral("idle"), QStringLiteral("lock_after_seconds")).toInt() > 0);

    connect(locks, &QCheckBox::toggled, this, [this]() { write(); });

    auto* form = new QFormLayout(this);
    form->setLabelAlignment(Qt::AlignRight | Qt::AlignTop);
    form->setFormAlignment(Qt::AlignHCenter | Qt::AlignTop);
    form->setVerticalSpacing(14);
    form->addRow(word("energy_screen_off", "Switch the screen off after:"), screen_off_row);
    form->addRow(word("energy_sleep", "Put the computer to sleep after:"), sleep_row);
    form->addRow(QString(), locks);

    const QString note = word("energy_lock_note", "");
    if (!note.isEmpty())
    {
        auto* note_label = new QLabel(note);
        note_label->setWordWrap(true);
        note_label->setEnabled(false);
        form->addRow(QString(), note_label);
    }
}

QHBoxLayout* EnergyPane::timeSlider(const QString& key, QSlider*& slider)
{
    const int chosen_seconds = choices.value(QStringLiteral("idle"), key).toInt();

    // The nearest stop: the config may name a time between two.
    int nearest = stops.size() - 1;
    if (chosen_seconds > 0)
    {
        nearest = 0;
        for (int stop = 0; stop < stops.size() - 1; stop++)
        {
            if (qAbs(stops[stop] - chosen_seconds) < qAbs(stops[nearest] - chosen_seconds))
            {
                nearest = stop;
            }
        }
    }

    slider = new QSlider(Qt::Horizontal);
    slider->setRange(0, stops.size() - 1);
    slider->setPageStep(1);
    slider->setTickPosition(QSlider::TicksBelow);
    slider->setTickInterval(1);
    slider->setFixedWidth(slider_width_px);
    slider->setValue(nearest);
    auto* time = new QLabel(written(stops[nearest]));

    connect(slider, &QSlider::valueChanged, this, [this, time](int stop) {
        time->setText(written(stops[stop]));
        write();
    });

    auto* row = new QHBoxLayout;
    row->setSpacing(10);
    row->addWidget(slider);
    row->addWidget(time);
    row->addStretch();
    return row;
}

void EnergyPane::write()
{
    const int screen_off_seconds = stops[screen_off->value()];
    choices.set(QStringLiteral("idle"), QStringLiteral("sleep_after_seconds"), stops[sleep->value()]);
    choices.set(QStringLiteral("idle"), QStringLiteral("screen_off_after_seconds"), screen_off_seconds);
    choices.set(
        QStringLiteral("idle"), QStringLiteral("lock_after_seconds"), locks->isChecked() ? screen_off_seconds : 0);
    choices.set(QStringLiteral("idle"), QStringLiteral("lock_before_sleep"), locks->isChecked());
}

QString EnergyPane::written(int seconds) const
{
    const auto word = [this](const char* key, const char* fallback) {
        return words.value(QLatin1String(key), QLatin1String(fallback)).toString();
    };

    if (seconds <= 0)
    {
        return word("energy_never", "Never");
    }

    const int minutes = seconds / seconds_per_minute;
    const int hours = minutes / minutes_per_hour;
    const int minutes_left = minutes % minutes_per_hour;

    QStringList parts;
    if (hours == 1)
    {
        parts << word("energy_hour", "%1 hour").arg(hours);
    }
    else if (hours > 1)
    {
        parts << word("energy_hours", "%1 hours").arg(hours);
    }
    if (minutes_left == 1)
    {
        parts << word("energy_minute", "%1 minute").arg(minutes_left);
    }
    else if (minutes_left > 1)
    {
        parts << word("energy_minutes", "%1 minutes").arg(minutes_left);
    }

    return parts.join(QLatin1Char(' '));
}
