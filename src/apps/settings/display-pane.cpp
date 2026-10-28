#include "display-pane.hpp"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCall>
#include <QDir>
#include <QFile>
#include <QComboBox>
#include <QFormLayout>
#include <QGuiApplication>
#include <QLabel>
#include <QRadioButton>
#include <QScreen>
#include <QSignalBlocker>
#include <QSlider>
#include <QTimer>
#include <QVBoxLayout>

#include <cmath>

#include "choices.hpp"
#include "display-modes.hpp"
#include "pane-parts.hpp"

// The screen's backlight, as the kernel offers it: the first one under
// /sys/class/backlight. Empty if the screen has none (most desktop screens).
static QString backlight_name()
{
    const QStringList names = QDir(QStringLiteral("/sys/class/backlight")).entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    return names.isEmpty() ? QString() : names.first();
}

static int backlight_value(const QString& name, const char* file)
{
    QFile reading(QStringLiteral("/sys/class/backlight/%1/%2").arg(name, QLatin1String(file)));
    return reading.open(QIODevice::ReadOnly) ? reading.readAll().trimmed().toInt() : 0;
}

// The share of the full brightness, from 0 to 100.
static int backlight_percent(const QString& name)
{
    const int largest = backlight_value(name, "max_brightness");
    return largest > 0 ? qRound(100.0 * backlight_value(name, "brightness") / largest) : 0;
}

// Through the login manager, which lets the person at the computer set the
// backlight without being an administrator.
static void set_backlight_percent(const QString& name, int percent)
{
    const int largest = backlight_value(name, "max_brightness");
    QDBusMessage setting = QDBusMessage::createMethodCall(QStringLiteral("org.freedesktop.login1"),
        QStringLiteral("/org/freedesktop/login1/session/auto"), QStringLiteral("org.freedesktop.login1.Session"),
        QStringLiteral("SetBrightness"));
    setting << QStringLiteral("backlight") << name << uint(qRound(largest * percent / 100.0));
    QDBusConnection::systemBus().asyncCall(setting);
}

// The scales on offer, from everything drawn largest to most room.
static const double scale_stops[] = {2.0, 1.75, 1.5, 1.25, 1.0};
static const int slider_width_px = 260;
// How long after the last step made with the keys (or a press on the
// slider's track) the scale is written. Long enough for a second press of
// the key to take the first one's place, short enough to feel direct.
static const int write_delay_ms = 500;

DisplayPane::DisplayPane(Choices& choices, QWidget* parent) : QWidget(parent), choices(choices)
{
    words = choices.configTable(QStringLiteral("settings")).value(QStringLiteral("words")).toMap();
    const auto word = [this](const char* key, const char* fallback) {
        return words.value(QLatin1String(key), QLatin1String(fallback)).toString();
    };

    for (const double stop : scale_stops)
    {
        stops.append(stop);
    }

    // The screen this window is on, in its own pixels: its size as programs
    // see it, times how much larger than its pixels it is drawn now.
    const QScreen* screen = QGuiApplication::primaryScreen();
    const double drawn_scale = screen->devicePixelRatio();
    screen_pixels =
        QSize(std::lround(screen->size().width() * drawn_scale), std::lround(screen->size().height() * drawn_scale));

    const QVariant held = choices.value(QStringLiteral("display"), QStringLiteral("scale"));
    const bool is_automatic = !held.isValid() || (held.toString() == QLatin1String("auto"));

    automatic = new QRadioButton(word("display_automatic", "Automatic, for this screen"));
    chosen = new QRadioButton(word("display_chosen", "Custom"));
    automatic->setChecked(is_automatic);
    chosen->setChecked(!is_automatic);

    // The slider stands at the scale that holds: the chosen one, or the one
    // the desktop decided on.
    const double shown_scale = is_automatic ? drawn_scale : held.toDouble();
    int nearest = 0;
    for (int stop = 0; stop < stops.size(); stop++)
    {
        if (std::abs(stops[stop] - shown_scale) < std::abs(stops[nearest] - shown_scale))
        {
            nearest = stop;
        }
    }

    scale = new QSlider(Qt::Horizontal);
    scale->setRange(0, stops.size() - 1);
    scale->setPageStep(1);
    scale->setTickPosition(QSlider::TicksBelow);
    scale->setTickInterval(1);
    scale->setFixedWidth(slider_width_px);
    scale->setValue(nearest);
    scale->setEnabled(!is_automatic);

    looks_like = new QLabel;
    looks_like->setEnabled(false);
    showLooksLike();

    connect(automatic, &QRadioButton::toggled, this, [this](bool on) {
        scale->setEnabled(!on);
        write_later->stop();
        write();
    });
    write_later = new QTimer(this);
    write_later->setSingleShot(true);
    write_later->setInterval(write_delay_ms);
    connect(write_later, &QTimer::timeout, this, &DisplayPane::write);

    // While the slider is dragged only the words under it follow; the
    // scale is written when it is let go.
    connect(scale, &QSlider::valueChanged, this, [this]() {
        showLooksLike();
        if (!scale->isSliderDown())
        {
            write_later->start();
        }
    });
    connect(scale, &QSlider::sliderReleased, this, [this]() {
        write_later->stop();
        write();
    });

    auto* choice = new QWidget;
    auto* rows = new QVBoxLayout(choice);
    rows->setContentsMargins(0, 0, 0, 0);
    rows->setSpacing(8);
    rows->addWidget(automatic);
    rows->addWidget(chosen);
    rows->addWidget(with_end_words(scale, word("display_larger", "Larger"), word("display_smaller", "More room")));
    rows->addWidget(looks_like);

    const QString note = word("display_note", "");
    if (!note.isEmpty())
    {
        auto* note_label = new QLabel(note);
        note_label->setWordWrap(true);
        note_label->setEnabled(false);
        rows->addWidget(note_label);
    }

    auto* form = new QFormLayout(this);
    form->setLabelAlignment(Qt::AlignRight | Qt::AlignTop);
    form->setFormAlignment(Qt::AlignHCenter | Qt::AlignTop);
    // The screen's resolution and refresh rate come first: what is drawn
    // larger or smaller is made of those pixels.
    auto* modes = new DisplayModes(choices, this);
    modes->addRowsTo(form);
    connect(modes, &DisplayModes::modeChanged, this, [this](const QString& name, QSize pixels) {
        if (this->screen() != nullptr && this->screen()->name() == name)
        {
            screen_pixels = pixels;
            showLooksLike();
        }
    });
    form->addRow(word("display_size", "Size of text and pictures:"), choice);

    // With several screens: which one has the menu bar and the dock, and
    // where each other one stands next to it. Takes effect at once; the
    // compositor reads the positions ([display.screens.NAME] position).
    const QList<QScreen*> screens = QGuiApplication::screens();
    if (screens.size() > 1)
    {
        const QString wanted_main = choices.value(QStringLiteral("display"), QStringLiteral("main_screen")).toString();
        auto* main_choice = new QComboBox;
        for (QScreen* one : screens)
        {
            main_choice->addItem(QStringLiteral("%1 (%2)").arg(one->model().isEmpty() ? one->name() : one->model(), one->name()),
                one->name());
        }
        main_choice->setCurrentIndex(qMax(0, main_choice->findData(wanted_main)));
        form->addRow(word("display_main", "Menu bar and dock on:"), main_choice);

        const struct
        {
            const char* key;
            const char* word_key;
            const char* fallback;
        } places[] = {
            {"right", "display_place_right", "right of the main screen"},
            {"left", "display_place_left", "left of the main screen"},
            {"above", "display_place_above", "above the main screen"},
            {"below", "display_place_below", "below the main screen"},
        };
        auto* arrangement = new QWidget;
        auto* arrangement_rows = new QFormLayout(arrangement);
        arrangement_rows->setContentsMargins(0, 0, 0, 0);
        QList<QComboBox*> place_choices;
        for (QScreen* one : screens)
        {
            auto* place = new QComboBox;
            for (const auto& choice : places)
            {
                place->addItem(word(choice.word_key, choice.fallback), QLatin1String(choice.key));
            }
            place->setProperty("screen", one->name());
            place_choices << place;
            arrangement_rows->addRow(one->name() + QLatin1Char(':'), place);
        }

        // The main screen at 0,0 and every other one beside it, in logical pixels.
        const auto arrange = [this, main_choice, place_choices]() {
            const QString main_name = main_choice->currentData().toString();
            QScreen* main_screen = nullptr;
            for (QScreen* one : QGuiApplication::screens())
            {
                if (one->name() == main_name)
                {
                    main_screen = one;
                }
            }
            if (main_screen == nullptr)
            {
                return;
            }
            QVariantMap per_screen = this->choices.value(QStringLiteral("display"), QStringLiteral("screens")).toMap();
            const QSize main_size = main_screen->size();
            for (QComboBox* place : place_choices)
            {
                const QString name = place->property("screen").toString();
                place->setEnabled(name != main_name);
                QPoint at(0, 0);
                QScreen* one = nullptr;
                for (QScreen* candidate : QGuiApplication::screens())
                {
                    if (candidate->name() == name)
                    {
                        one = candidate;
                    }
                }
                if (name != main_name && one != nullptr)
                {
                    const QString side = place->currentData().toString();
                    at = side == QLatin1String("left")    ? QPoint(-one->size().width(), 0)
                         : side == QLatin1String("above") ? QPoint(0, -one->size().height())
                         : side == QLatin1String("below") ? QPoint(0, main_size.height())
                                                          : QPoint(main_size.width(), 0);
                }
                QVariantMap screen = per_screen.value(name).toMap();
                screen.insert(QStringLiteral("position"), QStringLiteral("%1,%2").arg(at.x()).arg(at.y()));
                per_screen.insert(name, screen);
            }
            this->choices.set(QStringLiteral("display"), QStringLiteral("main_screen"), main_name);
            this->choices.set(QStringLiteral("display"), QStringLiteral("screens"), per_screen);
        };
        // Where each screen stands now, read back from the saved positions.
        const QVariantMap saved = choices.value(QStringLiteral("display"), QStringLiteral("screens")).toMap();
        for (QComboBox* place : place_choices)
        {
            const QStringList at = saved.value(place->property("screen").toString()).toMap()
                                       .value(QStringLiteral("position")).toString().split(QLatin1Char(','));
            if (at.size() == 2)
            {
                const int x = at[0].toInt();
                const int y = at[1].toInt();
                const char* side = x < 0 ? "left" : (y < 0 ? "above" : (y > 0 && x == 0 ? "below" : "right"));
                place->setCurrentIndex(qMax(0, place->findData(QLatin1String(side))));
            }
            place->setEnabled(place->property("screen").toString() != main_choice->currentData().toString());
            connect(place, &QComboBox::activated, this, arrange);
        }
        connect(main_choice, &QComboBox::activated, this, arrange);
        form->addRow(word("display_arrangement", "Arrangement:"), arrangement);
    }

    // Brightness, for a screen with a backlight; the brightness keys change it too.
    backlight = backlight_name();
    if (!backlight.isEmpty())
    {
        brightness = new QSlider(Qt::Horizontal);
        // Not down to 0, which would leave a black screen and no way to see the slider.
        brightness->setRange(5, 100);
        brightness->setMinimumWidth(220);
        brightness->setValue(backlight_percent(backlight));
        connect(brightness, &QSlider::valueChanged, this, [this](int percent) { set_backlight_percent(backlight, percent); });
        form->addRow(word("display_brightness", "Brightness:"),
            with_end_words(brightness, word("display_brightness_low", "Dim"), word("display_brightness_high", "Bright")));
    }

    // The lists were made last but stand first.
    QList<QWidget*> tab_order = modes->lists();
    tab_order << automatic << chosen << scale;
    for (int index = 1; index < tab_order.size(); index++)
    {
        setTabOrder(tab_order[index - 1], tab_order[index]);
    }
}

void DisplayPane::write()
{
    if (automatic->isChecked())
    {
        choices.set(QStringLiteral("display"), QStringLiteral("scale"), QStringLiteral("auto"));
    }
    else
    {
        choices.set(QStringLiteral("display"), QStringLiteral("scale"), stops[scale->value()]);
    }
}

void DisplayPane::showLooksLike()
{
    const double at = stops[scale->value()];
    const QString text =
        words.value(QStringLiteral("display_looks_like"), QStringLiteral("Room for %1 × %2")).toString();
    looks_like->setText(
        text.arg(std::lround(screen_pixels.width() / at)).arg(std::lround(screen_pixels.height() / at)));
}

void DisplayPane::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
    // The keys may have changed it while the pane was away.
    if (brightness != nullptr)
    {
        const QSignalBlocker quiet(brightness);
        brightness->setValue(backlight_percent(backlight));
    }
}
