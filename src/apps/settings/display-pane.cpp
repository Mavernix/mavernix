#include "display-pane.hpp"

#include <QFormLayout>
#include <QGuiApplication>
#include <QLabel>
#include <QRadioButton>
#include <QScreen>
#include <QSlider>
#include <QTimer>
#include <QVBoxLayout>

#include <cmath>

#include "choices.hpp"
#include "pane-parts.hpp"

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

    automatic = new QRadioButton(word("display_automatic", "Decide by the screen"));
    chosen = new QRadioButton(word("display_chosen", "Choose"));
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
    form->addRow(word("display_size", "Size:"), choice);
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
