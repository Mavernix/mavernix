#include "display-pane.hpp"

#include <QCheckBox>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCall>
#include <QDir>
#include <QFile>
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
#include "screen-arrangement.hpp"
#include "screen-layout.hpp"

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
// While the brightness slider is dragged, how often the screen follows it.
static const int brightness_step_ms = 50;
// Screens plugged in or taken out, and their sizes changing, come in bursts.
static const int screens_settle_ms = 300;

DisplayPane::DisplayPane(Choices& choices, QWidget* parent) : QWidget(parent), choices(choices)
{
    words = choices.configTable(QStringLiteral("settings")).value(QStringLiteral("words")).toMap();
    for (const double stop : scale_stops)
    {
        stops.append(stop);
    }

    // With several screens: the picture of them, and whose settings follow.
    arrangement = new ScreenArrangement;
    arrangement_note = new QLabel;
    arrangement_note->setEnabled(false);
    arrangement_note->setAlignment(Qt::AlignHCenter);
    arrangement_note->setText(in_lines_of(word("display_arrangement_note",
        "Click a screen to change its settings below. Drag the screens to stand as they do on your desk, "
        "and drag the bar along the top of one to move the menu bar and dock to another screen."),
        arrangement_note->fontMetrics(), intro_width_px));
    one_screen_note = new QLabel;
    one_screen_note->setEnabled(false);
    one_screen_note->setAlignment(Qt::AlignHCenter);
    one_screen_note->setText(in_lines_of(word("display_one_screen",
        "Plug in another screen to arrange your screens and choose each one's settings here."),
        one_screen_note->fontMetrics(), intro_width_px));
    picked_title = new QLabel;
    QFont title_font = picked_title->font();
    title_font.setBold(true);
    picked_title->setFont(title_font);

    automatic = new QRadioButton(word("display_automatic", "Automatic, for this screen"));
    chosen = new QRadioButton(word("display_chosen", "Custom"));
    scale = new QSlider(Qt::Horizontal);
    scale->setRange(0, stops.size() - 1);
    scale->setPageStep(1);
    scale->setTickPosition(QSlider::TicksBelow);
    scale->setTickInterval(1);
    scale->setFixedWidth(slider_width_px);
    looks_like = new QLabel;
    looks_like->setEnabled(false);

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
    auto* choice_rows = new QVBoxLayout(choice);
    choice_rows->setContentsMargins(0, 0, 0, 0);
    choice_rows->setSpacing(8);
    choice_rows->addWidget(automatic);
    choice_rows->addWidget(chosen);
    choice_rows->addWidget(with_end_words(scale, word("display_larger", "Larger"), word("display_smaller", "More room")));
    choice_rows->addWidget(looks_like);

    const QString note = word("display_note", "");
    if (!note.isEmpty())
    {
        // Broken by hand: a label that wraps by itself asks for far more height.
        auto* note_label = new QLabel;
        note_label->setEnabled(false);
        note_label->setText(in_lines_of(note, note_label->fontMetrics(), 260));
        choice_rows->addWidget(note_label);
    }

    main_here = new QCheckBox(word("display_main_here", "Show the menu bar and dock on this screen"));
    connect(main_here, &QCheckBox::toggled, this, [this](bool on) {
        if (on)
        {
            this->choices.set(QStringLiteral("display"), QStringLiteral("main_screen"), picked);
            arrangement->setMain(picked);
            // Another screen is made the main one from there, not by unticking this one.
            main_here->setEnabled(false);
        }
    });

    auto* rows = new QVBoxLayout(this);
    rows->setContentsMargins(0, 0, 0, 0);
    rows->setSpacing(pane_spacing_px);
    rows->addWidget(arrangement, 0, Qt::AlignHCenter);
    rows->addWidget(arrangement_note, 0, Qt::AlignHCenter);
    rows->addWidget(picked_title, 0, Qt::AlignHCenter);
    form = new QFormLayout;
    form->setLabelAlignment(Qt::AlignRight | Qt::AlignTop);
    form->setFormAlignment(Qt::AlignHCenter | Qt::AlignTop);
    rows->addLayout(form);
    rows->addWidget(one_screen_note, 0, Qt::AlignHCenter);
    rows->addStretch();

    // The screen's resolution and refresh rate come first: what is drawn
    // larger or smaller is made of those pixels.
    modes = new DisplayModes(choices, this);
    modes->addRowsTo(form);
    connect(modes, &DisplayModes::modeChanged, this, [this](const QString& name, QSize pixels) {
        if (name == picked)
        {
            screen_pixels = pixels;
            showLooksLike();
        }
        tidyLater();
    });
    form->addRow(word("display_size", "Size of text and pictures:"), choice);
    form->addRow(QString(), main_here);

    // Brightness, for a screen with a backlight; the brightness keys change it too.
    backlight = backlight_name();
    if (!backlight.isEmpty())
    {
        brightness = new QSlider(Qt::Horizontal);
        // Not down to 0, which would leave a black screen and no way to see the slider.
        brightness->setRange(5, 100);
        brightness->setMinimumWidth(220);
        brightness->setValue(backlight_percent(backlight));
        follow_slider(brightness, brightness_step_ms, [this](int percent) { set_backlight_percent(backlight, percent); });
        brightness_row = with_end_words(brightness, word("display_brightness_low", "Dim"), word("display_brightness_high", "Bright"));
        form->addRow(word("display_brightness", "Brightness:"), brightness_row);
    }

    connect(arrangement, &ScreenArrangement::picked, this, &DisplayPane::pick);
    connect(arrangement, &ScreenArrangement::arranged, this,
        [this](const QMap<QString, QRect>& screens) { writePositions(positions(screens, mainName())); });
    connect(arrangement, &ScreenArrangement::mainChosen, this, [this](const QString& name) {
        this->choices.set(QStringLiteral("display"), QStringLiteral("main_screen"), name);
        pick(picked);
    });

    // Screens plugged in, taken out, rescaled or moved.
    read_later = new QTimer(this);
    read_later->setSingleShot(true);
    read_later->setInterval(screens_settle_ms);
    connect(read_later, &QTimer::timeout, this, &DisplayPane::readScreens);
    const auto watch = [this](QScreen* screen) {
        connect(screen, &QScreen::geometryChanged, read_later, qOverload<>(&QTimer::start));
    };
    for (QScreen* screen : QGuiApplication::screens())
    {
        watch(screen);
    }
    connect(qApp, &QGuiApplication::screenAdded, this, [this, watch](QScreen* screen) {
        watch(screen);
        read_later->start();
        modes->ask();
    });
    connect(qApp, &QGuiApplication::screenRemoved, this, [this]() { read_later->start(); });

    readScreens();
    pick(mainName());

    QList<QWidget*> tab_order = modes->lists();
    tab_order << automatic << chosen << scale << main_here;
    for (int index = 1; index < tab_order.size(); index++)
    {
        setTabOrder(tab_order[index - 1], tab_order[index]);
    }
}

QString DisplayPane::word(const char* key, const char* fallback) const
{
    return words.value(QLatin1String(key), QString::fromUtf8(fallback)).toString();
}

// What a screen is called in the pane: its model, or for a laptop's own
// panel just that. Two screens of one model are told apart by their names.
static QString label_of(const QScreen* screen, const QString& built_in_word)
{
    if (is_built_in(screen->name()))
    {
        return built_in_word;
    }
    // Qt says "Unknown" for a screen that does not tell.
    const QString model = screen->model();
    if (model.isEmpty() || model == QLatin1String("Unknown"))
    {
        return screen->name();
    }
    for (const QScreen* other : QGuiApplication::screens())
    {
        if (other != screen && other->model() == model)
        {
            return QStringLiteral("%1 (%2)").arg(model, screen->name());
        }
    }
    return model;
}

QString DisplayPane::mainName() const
{
    // As the shell decides: [display] main_screen if it is plugged in, else the first.
    const QString wanted = choices.value(QStringLiteral("display"), QStringLiteral("main_screen")).toString();
    const QList<QScreen*> screens = QGuiApplication::screens();
    for (const QScreen* screen : screens)
    {
        if (screen->name() == wanted)
        {
            return wanted;
        }
    }
    return screens.isEmpty() ? QString() : screens.first()->name();
}

QScreen* DisplayPane::pickedScreen() const
{
    for (QScreen* screen : QGuiApplication::screens())
    {
        if (screen->name() == picked)
        {
            return screen;
        }
    }
    return nullptr;
}

void DisplayPane::readScreens()
{
    const QList<QScreen*> found = QGuiApplication::screens();
    const QString built_in_word = word("display_built_in", "Built-in screen");
    QList<ArrangedScreen> shown;
    QMap<QString, QRect> now;
    for (const QScreen* screen : found)
    {
        shown.append({screen->name(), label_of(screen, built_in_word), screen->geometry()});
        now.insert(screen->name(), screen->geometry());
    }
    arrangement->setScreens(shown);
    arrangement->setMain(mainName());

    const bool several = found.size() > 1;
    arrangement->setVisible(several);
    arrangement_note->setVisible(several);
    picked_title->setVisible(several);
    one_screen_note->setVisible(!several);
    form->setRowVisible(main_here, several);

    if (tidy_pending)
    {
        tidy_pending = false;
        const QMap<QString, QRect> tidy = tidied(now, mainName());
        if (tidy != now)
        {
            writePositions(positions(tidy, mainName()));
        }
    }
    // The picked screen was taken out, or this is the first reading.
    if (pickedScreen() == nullptr)
    {
        pick(mainName());
    }
}

void DisplayPane::pick(const QString& name)
{
    picked = name;
    arrangement->setPicked(name);
    const QScreen* screen = pickedScreen();
    if (screen == nullptr)
    {
        return;
    }
    picked_title->setText(label_of(screen, word("display_built_in", "Built-in screen")));

    // In its own pixels: its size as programs see it, times how much larger
    // than its pixels it is drawn now.
    const double drawn_scale = screen->devicePixelRatio();
    screen_pixels =
        QSize(std::lround(screen->size().width() * drawn_scale), std::lround(screen->size().height() * drawn_scale));

    // Its own scale, else the one for every screen.
    const QVariant own = choices.value(QStringLiteral("display"), QStringLiteral("screens"))
                             .toMap().value(name).toMap().value(QStringLiteral("scale"));
    const QVariant held = own.isValid() ? own : choices.value(QStringLiteral("display"), QStringLiteral("scale"));
    const bool is_automatic = !held.isValid() || (held.toString() == QLatin1String("auto"));
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
    {
        const QSignalBlocker quiet_automatic(automatic);
        const QSignalBlocker quiet_chosen(chosen);
        const QSignalBlocker quiet_scale(scale);
        automatic->setChecked(is_automatic);
        chosen->setChecked(!is_automatic);
        scale->setValue(nearest);
        scale->setEnabled(!is_automatic);
    }
    showLooksLike();
    modes->showScreenNamed(name);

    {
        const QSignalBlocker quiet(main_here);
        const bool is_main = name == mainName();
        main_here->setChecked(is_main);
        main_here->setEnabled(!is_main);
    }
    // The backlight is the built-in panel's.
    if (brightness_row != nullptr)
    {
        form->setRowVisible(brightness_row, QGuiApplication::screens().size() < 2 || is_built_in(name));
    }
}

void DisplayPane::write()
{
    // The whole table is written: a screen's other keys stay as they are.
    QVariantMap all = choices.value(QStringLiteral("display"), QStringLiteral("screens")).toMap();
    QVariantMap own = all.value(picked).toMap();
    own.insert(QStringLiteral("scale"),
        automatic->isChecked() ? QVariant(QStringLiteral("auto")) : QVariant(stops[scale->value()]));
    all.insert(picked, own);
    choices.set(QStringLiteral("display"), QStringLiteral("screens"), all);
    tidyLater();
}

void DisplayPane::writePositions(const QMap<QString, QPoint>& positions)
{
    QVariantMap all = choices.value(QStringLiteral("display"), QStringLiteral("screens")).toMap();
    for (auto at = positions.constBegin(); at != positions.constEnd(); ++at)
    {
        QVariantMap own = all.value(at.key()).toMap();
        own.insert(QStringLiteral("position"), QStringLiteral("%1,%2").arg(at.value().x()).arg(at.value().y()));
        all.insert(at.key(), own);
    }
    choices.set(QStringLiteral("display"), QStringLiteral("screens"), all);
}

void DisplayPane::tidyLater()
{
    // Done once the compositor has drawn the screen at its new size.
    tidy_pending = QGuiApplication::screens().size() > 1;
}

void DisplayPane::showLooksLike()
{
    const double at = stops[scale->value()];
    const QString text = word("display_looks_like", "Room for %1 × %2");
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
