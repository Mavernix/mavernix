#include "energy-pane.hpp"

#include <QCheckBox>
#include <QComboBox>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusVariant>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QSlider>
#include <QTimer>

#include "choices.hpp"
#include "pane-parts.hpp"

// In minutes; "never" comes after them.
static const int stop_minutes[] = {1, 3, 5, 10, 15, 20, 25, 30, 45, 60, 75, 90, 105, 120, 180};
static const int seconds_per_minute = 60;
static const int minutes_per_hour = 60;
// Wide enough that neighbouring stops are easy to tell apart, narrow
// enough that the longer label and the longest time fit beside it.
static const int slider_width_px = 260;
// How long after the last step made with the keys the times are written.
// Each write of [idle] makes the session rewrite the compositor's config,
// so holding an arrow key must not write at every stop.
static const int write_delay_ms = 500;
EnergyPane::EnergyPane(Choices& choices, QWidget* parent) : QWidget(parent), choices(choices)
{
    words = choices.configTable(QStringLiteral("settings")).value(QStringLiteral("words")).toMap();
    const auto word = [this](const char* key, const char* fallback) {
        return words.value(QLatin1String(key), QLatin1String(fallback)).toString();
    };

    write_later = new QTimer(this);
    write_later->setSingleShot(true);
    write_later->setInterval(write_delay_ms);
    connect(write_later, &QTimer::timeout, this, &EnergyPane::write);

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

    // The screen switches off no later than the computer sleeps: moving one
    // slider past the other takes the other along. "Never" is the last stop.
    connect(sleep, &QSlider::valueChanged, this, [this](int stop) {
        if (screen_off->value() > stop)
        {
            screen_off->setValue(stop);
        }
    });
    connect(screen_off, &QSlider::valueChanged, this, [this](int stop) {
        if (sleep->value() < stop)
        {
            sleep->setValue(stop);
        }
    });
    // A config written by hand may have them the wrong way round: put right,
    // and so written, as a slider moved there would be.
    if (screen_off->value() > sleep->value())
    {
        screen_off->setValue(sleep->value());
    }
    connect(locks, &QCheckBox::toggled, this, [this]() { write(); });

    lock_note = new QLabel;
    lock_note->setWordWrap(true);
    lock_note->setFont(scaled_font(lock_note->font(), small_words_scale));
    lock_note->setFixedWidth(slider_width_px + 80);
    connect(screen_off, &QSlider::valueChanged, this, &EnergyPane::showLockNote);
    connect(sleep, &QSlider::valueChanged, this, &EnergyPane::showLockNote);
    connect(locks, &QCheckBox::toggled, this, &EnergyPane::showLockNote);

    auto* form = new QFormLayout(this);
    form->setLabelAlignment(Qt::AlignRight | Qt::AlignTop);
    form->setFormAlignment(Qt::AlignHCenter | Qt::AlignTop);
    form->setVerticalSpacing(14);
    form->addRow(word("energy_screen_off", "Switch the screen off after:"), screen_off_row);
    form->addRow(word("energy_sleep", "Put the computer to sleep after:"), sleep_row);
    form->addRow(QString(), locks);
    form->addRow(QString(), lock_note);
    showLockNote();

    // What the shell does with it (src/shell/Idle.qml); "sleep" is the
    // system's own rule, which usually sleeps.
    {
        auto* lid = new QComboBox;
        lid->addItem(word("energy_lid_sleep", "Sleep"), QStringLiteral("sleep"));
        lid->addItem(word("energy_lid_lock", "Lock the screen"), QStringLiteral("lock"));
        lid->addItem(word("energy_lid_nothing", "Do nothing"), QStringLiteral("nothing"));
        const QString chosen = choices.value(QStringLiteral("power"), QStringLiteral("lid_closed")).toString();
        lid->setCurrentIndex(qMax(0, lid->findData(chosen)));
        connect(lid, &QComboBox::activated, this, [this, lid]() {
            this->choices.set(QStringLiteral("power"), QStringLiteral("lid_closed"), lid->currentData());
        });
        // As wide as its longest choice, not as the pane.
        lid->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
        form->addRow(word("energy_lid", "When the lid is closed:"), lid);
        // Shown once UPower says this computer has a lid; asked aside, so
        // a slow or missing UPower does not hold the window up.
        form->setRowVisible(lid, false);
        QDBusMessage ask = QDBusMessage::createMethodCall(QStringLiteral("org.freedesktop.UPower"),
            QStringLiteral("/org/freedesktop/UPower"), QStringLiteral("org.freedesktop.DBus.Properties"),
            QStringLiteral("Get"));
        ask << QStringLiteral("org.freedesktop.UPower") << QStringLiteral("LidIsPresent");
        auto* watcher = new QDBusPendingCallWatcher(QDBusConnection::systemBus().asyncCall(ask), this);
        connect(watcher, &QDBusPendingCallWatcher::finished, this, [form, lid](QDBusPendingCallWatcher* finished) {
            const QDBusPendingReply<QDBusVariant> reply = *finished;
            finished->deleteLater();
            form->setRowVisible(lid, !reply.isError() && reply.value().variant().toBool());
        });
    }

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
    // As wide as the longest time, so the form does not shift as it changes.
    int widest_px = 0;
    for (const int seconds : stops)
    {
        widest_px = qMax(widest_px, time->fontMetrics().horizontalAdvance(written(seconds)));
    }
    time->setMinimumWidth(widest_px);

    // While the slider is dragged only the time beside it follows; the
    // times are written when it is let go.
    connect(slider, &QSlider::valueChanged, this, [this, time](int stop) {
        time->setText(written(stops[stop]));
        // Either slider: the other one moves along while one is dragged.
        if (!screen_off->isSliderDown() && !sleep->isSliderDown())
        {
            write_later->start();
        }
    });
    connect(slider, &QSlider::sliderReleased, this, [this]() {
        write_later->stop();
        write();
    });

    auto* row = new QHBoxLayout;
    row->setSpacing(10);
    row->addWidget(slider);
    row->addWidget(time);
    row->addStretch();
    return row;
}

EnergyPane::~EnergyPane()
{
    // A step made just before the window closed.
    if (write_later->isActive())
    {
        write();
    }
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

void EnergyPane::showLockNote()
{
    // "Never" is the last stop of both sliders.
    const int never = stops.size() - 1;
    const bool never_idle = screen_off->value() == never && sleep->value() == never;
    lock_note->setVisible(locks->isChecked() && never_idle);
    lock_note->setText(words.value(QStringLiteral("energy_lock_never"),
        QStringLiteral("The screen never switches off and the computer never sleeps, so it won't lock by itself. "
                       "Lock it any time from the menu at the right of the menu bar."))
                           .toString());
}
