#include "trackpad-pane.hpp"

#include <QCheckBox>
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

    auto* rows = new QVBoxLayout(this);
    rows->setSpacing(14);
    rows->addWidget(with_note(tap, word("trackpad_tap_note", "")));
    rows->addWidget(with_note(natural, word("trackpad_natural_scroll_note", "")));
}

void TrackpadPane::change(const QString& key, bool on)
{
    choices.set(QStringLiteral("input"), key, on);
    QProcess::startDetached(reconfigure_command, {});
}
