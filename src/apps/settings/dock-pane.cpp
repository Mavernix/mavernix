#include "dock-pane.hpp"

#include <QCheckBox>
#include <QComboBox>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QFormLayout>
#include <QGridLayout>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QRegularExpression>
#include <QScreen>
#include <QSignalBlocker>
#include <QSlider>
#include <QVBoxLayout>

#include "choices.hpp"
#include "pane-parts.hpp"

DockPane::DockPane(Choices& choices, QWidget* parent) : QWidget(parent), choices(choices)
{
    const QVariantMap words = choices.configTable(QStringLiteral("settings")).value(QStringLiteral("words")).toMap();
    const auto word = [&words](const char* key, const char* fallback) {
        return words.value(QLatin1String(key), QString::fromUtf8(fallback)).toString();
    };

    const QVariantMap dock_looks = choices.themeTable(QStringLiteral("dock"));
    const int smallest = dock_looks.value(QStringLiteral("smallest_icon_size"), 16).toInt();
    const int largest = dock_looks.value(QStringLiteral("largest_icon_size"), 128).toInt();
    // Wide enough to be set finely.
    const int slider_width = 200;

    size = new QSlider(Qt::Horizontal);
    size->setRange(smallest, largest);
    size->setValue(currentIconSize());
    size->setMinimumWidth(slider_width);
    connect(size, &QSlider::valueChanged, this,
        [this](int value) { this->choices.set(QStringLiteral("dock"), QStringLiteral("icon_size"), value); });

    magnification = new QCheckBox(word("dock_magnification", "Enlarge under the pointer:"));
    magnification->setChecked(choices.value(QStringLiteral("dock"), QStringLiteral("magnification")).toBool());

    magnified_size = new QSlider(Qt::Horizontal);
    magnified_size->setRange(smallest, largest);
    const QVariant chosen_largest = choices.value(QStringLiteral("dock"), QStringLiteral("largest_icon_size"));
    magnified_size->setValue(chosen_largest.isValid() ? chosen_largest.toInt() : largest);
    magnified_size->setMinimumWidth(slider_width);
    magnified_size->setEnabled(magnification->isChecked());
    connect(magnified_size, &QSlider::valueChanged, this,
        [this](int value) { this->choices.set(QStringLiteral("dock"), QStringLiteral("largest_icon_size"), value); });
    connect(magnification, &QCheckBox::toggled, this, [this](bool on) {
        magnified_size->setEnabled(on);
        this->choices.set(QStringLiteral("dock"), QStringLiteral("magnification"), on);
    });

    hides = new QCheckBox(word("dock_autohide", "Hide the dock until the pointer is at the screen's edge"));
    hides->setChecked(choices.value(QStringLiteral("dock"), QStringLiteral("autohide")).toBool());
    connect(hides, &QCheckBox::toggled, this,
        [this](bool on) { this->choices.set(QStringLiteral("dock"), QStringLiteral("autohide"), on); });

    // Labels in one column, ending where the controls begin.
    auto* form = new QFormLayout(this);
    form->setLabelAlignment(Qt::AlignRight | Qt::AlignTop);
    form->setFormAlignment(Qt::AlignHCenter | Qt::AlignTop);
    form->addRow(word("dock_size", "Size:"),
        with_end_words(size, word("dock_size_small", "Small"), word("dock_size_large", "Large")));
    form->addRow(magnification,
        with_end_words(magnified_size, word("dock_magnification_min", "Min"), word("dock_magnification_max", "Max")));
    form->addRow(QString(), hides);

    // Effects the original didn't have, or had otherwise; all off at first.
    // The compositor follows at once (the shell restarts for them).
    auto* reflections = new QCheckBox(word("dock_window_reflections", "Reflect windows in the Dock's glass"));
    reflections->setChecked(choices.value(QStringLiteral("dock"), QStringLiteral("window_reflections")).toBool());
    connect(reflections, &QCheckBox::toggled, this,
        [this](bool on) { this->choices.set(QStringLiteral("dock"), QStringLiteral("window_reflections"), on); });
    form->addRow(QString(), with_note(reflections, word("dock_window_reflections_note",
                                                         "Uses more of the graphics card while windows move.")));

    auto* dock_blur = new QCheckBox(word("dock_blur", "Blur behind the Dock"));
    dock_blur->setChecked(choices.value(QStringLiteral("blur"), QStringLiteral("dock_shelf")).toBool());
    connect(dock_blur, &QCheckBox::toggled, this,
        [this](bool on) { this->choices.set(QStringLiteral("blur"), QStringLiteral("dock_shelf"), on); });
    form->addRow(QString(), dock_blur);

    // The menu bar: which items show at its right end, and the clock's hours.
    // The shell follows at once.
    const QVariantMap item_names = words.value(QStringLiteral("menu_bar_items")).toMap();
    const QStringList listed = choices.value(QStringLiteral("panel"), QStringLiteral("right")).toStringList();
    // In two columns, so that the list stays short.
    auto* items = new QWidget;
    auto* item_rows = new QGridLayout(items);
    item_rows->setContentsMargins(0, 0, 0, 0);
    item_rows->setVerticalSpacing(2);
    item_rows->setHorizontalSpacing(18);
    const int per_column = (listed.size() + 1) / 2;
    int place = 0;
    for (const QString& name : listed)
    {
        // Items without a friendly name (a custom one) keep their own.
        auto* shown = new QCheckBox(item_names.value(name, name).toString());
        shown->setChecked(
            !choices.value(QStringLiteral("panel"), QStringLiteral("hidden")).toStringList().contains(name));
        connect(shown, &QCheckBox::toggled, this, [this, name](bool on) {
            QStringList hidden = this->choices.value(QStringLiteral("panel"), QStringLiteral("hidden")).toStringList();
            hidden.removeAll(name);
            if (!on)
            {
                hidden.append(name);
            }
            this->choices.set(QStringLiteral("panel"), QStringLiteral("hidden"), hidden);
        });
        item_rows->addWidget(shown, place % per_column, place / per_column);
        place++;
    }
    form->addRow(word("menu_bar_show", "Show in the menu bar:"), items);

    // Hours as 13:00 or as 1:00 PM; the rest of the theme's format stays.
    const QString format = choices.value(QStringLiteral("panel"), QStringLiteral("clock_format")).toString();
    auto* day_hours = new QCheckBox(word("menu_bar_24_hours", "Show the time with 24 hours"));
    day_hours->setChecked(!format.contains(QLatin1String("AP"), Qt::CaseInsensitive));
    connect(day_hours, &QCheckBox::toggled, this, [this](bool on) {
        QString changed = this->choices.value(QStringLiteral("panel"), QStringLiteral("clock_format")).toString();
        if (on)
        {
            changed.replace(QRegularExpression(QStringLiteral(" ?[Aa][Pp]")), QString());
            changed.replace(QRegularExpression(QStringLiteral("\\bh{1,2}\\b")), QStringLiteral("HH"));
        }
        else
        {
            changed.replace(QRegularExpression(QStringLiteral("\\bH{1,2}\\b")), QStringLiteral("h"));
            changed += QStringLiteral(" AP");
        }
        this->choices.set(QStringLiteral("panel"), QStringLiteral("clock_format"), changed);
    });
    form->addRow(QString(), day_hours);

    // Seconds after the minutes ("HH:mm:ss"), as on OS X; the clock then
    // ticks every second.
    const QRegularExpression seconds_part(QStringLiteral(":ss?\\b"));
    auto* seconds = new QCheckBox(word("menu_bar_seconds", "Show seconds"));
    seconds->setChecked(format.contains(seconds_part));
    connect(seconds, &QCheckBox::toggled, this, [this, seconds_part](bool on) {
        QString changed = this->choices.value(QStringLiteral("panel"), QStringLiteral("clock_format")).toString();
        changed.remove(seconds_part);
        if (on)
        {
            changed.replace(QRegularExpression(QStringLiteral("\\bmm\\b")), QStringLiteral("mm:ss"));
        }
        this->choices.set(QStringLiteral("panel"), QStringLiteral("clock_format"), changed);
    });
    form->addRow(QString(), seconds);

    // Only a translucent menu bar (the Desktop pane's checkbox) shows it.
    auto* menu_bar_blur = new QCheckBox(word("menu_bar_blur", "Blur menu bar"));
    menu_bar_blur->setChecked(choices.value(QStringLiteral("blur"), QStringLiteral("menu_bar")).toBool());
    connect(menu_bar_blur, &QCheckBox::toggled, this,
        [this](bool on) { this->choices.set(QStringLiteral("blur"), QStringLiteral("menu_bar"), on); });
    form->addRow(QString(), with_note(menu_bar_blur, word("menu_bar_blur_note",
                                                           "Shows with a translucent menu bar (Desktop).")));

    // The folder is watched too, for a choices file replaced whole or not
    // there yet (as in the Extensions pane).
    const QString choices_file = choices.userChoicesFile();
    watcher = new QFileSystemWatcher(this);
    watcher->addPath(QFileInfo(choices_file).absolutePath());
    auto follow = [this, choices_file]() {
        if (QFileInfo::exists(choices_file) && !watcher->files().contains(choices_file))
        {
            watcher->addPath(choices_file);
        }
        showCurrent();
    };
    follow();
    connect(watcher, &QFileSystemWatcher::fileChanged, this, follow);
    connect(watcher, &QFileSystemWatcher::directoryChanged, this, follow);
}

void DockPane::showCurrent()
{
    // Shown without being written back.
    const QSignalBlocker size_blocker(size);
    const QSignalBlocker magnification_blocker(magnification);
    const QSignalBlocker magnified_size_blocker(magnified_size);
    const QSignalBlocker hides_blocker(hides);

    size->setValue(currentIconSize());
    magnification->setChecked(choices.current(QStringLiteral("dock"), QStringLiteral("magnification")).toBool());
    magnified_size->setEnabled(magnification->isChecked());
    const QVariant chosen_largest = choices.current(QStringLiteral("dock"), QStringLiteral("largest_icon_size"));
    if (chosen_largest.isValid())
    {
        magnified_size->setValue(chosen_largest.toInt());
    }
    hides->setChecked(choices.current(QStringLiteral("dock"), QStringLiteral("autohide")).toBool());
}

int DockPane::currentIconSize() const
{
    const QVariant chosen = choices.current(QStringLiteral("dock"), QStringLiteral("icon_size"));
    if (chosen.isValid())
    {
        return chosen.toInt();
    }

    // The theme's size, or its smaller one on a low screen: the dock's own
    // rule (src/shell/DockLayout.js, iconSize).
    const QVariantMap looks = choices.themeTable(QStringLiteral("dock"));
    const int screen_height = QGuiApplication::primaryScreen()->size().height();
    if (looks.contains(QStringLiteral("compact_icon_size")) &&
        screen_height <= looks.value(QStringLiteral("compact_up_to_height")).toInt())
    {
        return looks.value(QStringLiteral("compact_icon_size")).toInt();
    }

    return looks.value(QStringLiteral("icon_size")).toInt();
}
