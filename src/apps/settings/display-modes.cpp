#include "display-modes.hpp"

#include <QComboBox>
#include <QFormLayout>
#include <QJsonArray>
#include <QJsonObject>
#include <QMessageBox>
#include <QPushButton>
#include <QSignalBlocker>
#include <QTimer>
#include <QWidget>

#include <algorithm>
#include <cstdlib>

#include "choices.hpp"
#include "compositor-call.hpp"

// How long a new mode is shown before it is undone, unless kept.
static const int keep_seconds = 15;
static const int millihertz_per_hertz = 1000;

static ScreenMode mode_from_json(const QJsonObject& json)
{
    ScreenMode mode;
    mode.width = json.value(QStringLiteral("width")).toInt();
    mode.height = json.value(QStringLiteral("height")).toInt();
    mode.refresh = json.value(QStringLiteral("refresh")).toInt();
    mode.preferred = json.value(QStringLiteral("preferred")).toBool();
    return mode;
}

// As the compositor's config writes it: "1920x1080@60000".
static QString written(int width, int height, int refresh)
{
    return QStringLiteral("%1x%2@%3").arg(width).arg(height).arg(refresh);
}

DisplayModes::DisplayModes(Choices& choices, QWidget* pane) : QObject(pane), choices(choices), pane(pane)
{
    words = choices.configTable(QStringLiteral("settings")).value(QStringLiteral("words")).toMap();

    resolution_list = new QComboBox;
    refresh_list = new QComboBox;

    connect(resolution_list, &QComboBox::activated, this, [this]() {
        showRefreshRates();
        choose();
    });
    connect(refresh_list, &QComboBox::activated, this, &DisplayModes::choose);
    ask();
}

void DisplayModes::ask()
{
    ask_compositor(
        QStringLiteral("screen-modes/list"), this, [this](const QJsonObject& answer, const QString& problem) {
        if (!problem.isEmpty())
        {
            qInfo("mavernix-settings: the screens' modes are not known (%s)", qPrintable(problem));
            return;
        }
        QList<ModedScreen> found;
        for (const QJsonValue& entry : answer.value(QStringLiteral("screens")).toArray())
        {
            const QJsonObject json = entry.toObject();
            ModedScreen screen;
            screen.name = json.value(QStringLiteral("name")).toString();
            screen.description = json.value(QStringLiteral("description")).toString();
            screen.current = mode_from_json(json.value(QStringLiteral("current")).toObject());
            QList<ScreenMode> reported;
            for (const QJsonValue& mode : json.value(QStringLiteral("modes")).toArray())
            {
                reported.append(mode_from_json(mode.toObject()));
            }
            screen.modes = offered_modes(screen.name, screen.current, reported);
            found.append(screen);
        }
        showScreens(found);
    });
}

void DisplayModes::addRowsTo(QFormLayout* rows)
{
    form = rows;
    form->addRow(word("display_resolution", "Resolution:"), resolution_list);
    form->addRow(word("display_refresh", "Refresh rate:"), refresh_list);
    form->setRowVisible(resolution_list, false);
    form->setRowVisible(refresh_list, false);
}

QList<QWidget*> DisplayModes::lists() const
{
    return {resolution_list, refresh_list};
}

void DisplayModes::showScreenNamed(const QString& name)
{
    shown_name = name;
    showScreen();
}

int DisplayModes::shownIndex() const
{
    for (int index = 0; index < screens.size(); index++)
    {
        if (screens[index].name == shown_name)
        {
            return index;
        }
    }
    return -1;
}

void DisplayModes::showScreens(const QList<ModedScreen>& found)
{
    screens.clear();
    for (const ModedScreen& screen : found)
    {
        if (!screen.modes.isEmpty())
        {
            screens.append(screen);
        }
    }
    showScreen();
}

void DisplayModes::showScreen()
{
    // A virtual screen, or one the compositor has not told of yet, has no rows.
    const int index = shownIndex();
    if (form != nullptr)
    {
        form->setRowVisible(resolution_list, index >= 0);
        form->setRowVisible(refresh_list, index >= 0);
    }
    if (index < 0)
    {
        return;
    }
    const ModedScreen& screen = screens[index];
    const QString recommended = word("display_recommended", "%1 (recommended)");

    // Each size once, the most pixels first.
    QList<QSize> sizes;
    QList<QSize> preferred_sizes;
    for (const ScreenMode& mode : screen.modes)
    {
        const QSize size(mode.width, mode.height);
        if (!sizes.contains(size))
        {
            sizes.append(size);
        }
        if (mode.preferred)
        {
            preferred_sizes.append(size);
        }
    }
    std::sort(sizes.begin(), sizes.end(), [](const QSize& one, const QSize& other) {
        const qint64 one_pixels = qint64(one.width()) * one.height();
        const qint64 other_pixels = qint64(other.width()) * other.height();
        return one_pixels != other_pixels ? one_pixels > other_pixels : one.width() > other.width();
    });

    const QSignalBlocker quiet(resolution_list);
    resolution_list->clear();
    for (const QSize& size : sizes)
    {
        const QString text = QStringLiteral("%1 × %2").arg(size.width()).arg(size.height());
        resolution_list->addItem(preferred_sizes.contains(size) ? recommended.arg(text) : text, size);
    }
    resolution_list->setCurrentIndex(resolution_list->findData(QSize(screen.current.width, screen.current.height)));
    showRefreshRates();
}

void DisplayModes::showRefreshRates()
{
    const int index = shownIndex();
    if (index < 0)
    {
        return;
    }
    const ModedScreen& screen = screens[index];
    const QSize size = resolution_list->currentData().toSize();
    const QString hertz = word("display_hertz", "%1 Hz");

    QList<int> rates;
    for (const ScreenMode& mode : screen.modes)
    {
        if (QSize(mode.width, mode.height) == size && !rates.contains(mode.refresh))
        {
            rates.append(mode.refresh);
        }
    }
    std::sort(rates.begin(), rates.end(), std::greater<int>());

    const QSignalBlocker quiet(refresh_list);
    refresh_list->clear();
    for (const int rate : rates)
    {
        // 60000 is "60 Hz", 59940 "59.94 Hz".
        const QString number = rate % millihertz_per_hertz == 0
                                   ? QString::number(rate / millihertz_per_hertz)
                                   : QString::number(double(rate) / millihertz_per_hertz, 'f', 2);
        refresh_list->addItem(hertz.arg(number), rate);
    }
    // The current rate if this is the current size, else the highest. A
    // custom mode's rate may come back a hair off the one asked: the nearest.
    int shown = 0;
    if (size == QSize(screen.current.width, screen.current.height))
    {
        for (int index = 1; index < rates.size(); index++)
        {
            if (std::abs(rates[index] - screen.current.refresh) < std::abs(rates[shown] - screen.current.refresh))
            {
                shown = index;
            }
        }
    }
    refresh_list->setCurrentIndex(shown);
    refresh_list->setEnabled(refresh_list->count() > 1);
}

void DisplayModes::choose()
{
    const int index = shownIndex();
    if (index < 0)
    {
        return;
    }
    ModedScreen& screen = screens[index];
    const QSize size = resolution_list->currentData().toSize();
    const int refresh = refresh_list->currentData().toInt();
    // The rate shown for the current mode may be a hair off it (showRefreshRates).
    if (size == QSize(screen.current.width, screen.current.height) &&
        std::abs(refresh - screen.current.refresh) <= same_rate_millihertz)
    {
        return;
    }

    const QString name = screen.name;
    const QString before = writtenMode(name);
    writeMode(name, written(size.width(), size.height(), refresh));
    if (keepAsked())
    {
        screen.current.width = size.width();
        screen.current.height = size.height();
        screen.current.refresh = refresh;
        emit modeChanged(name, size);
    }
    else
    {
        writeMode(name, before);
        showScreen();
    }
}

QString DisplayModes::writtenMode(const QString& name) const
{
    const QVariantMap all = choices.value(QStringLiteral("display"), QStringLiteral("screens")).toMap();
    return all.value(name).toMap().value(QStringLiteral("mode")).toString();
}

void DisplayModes::writeMode(const QString& name, const QString& mode)
{
    // The whole table is written: a screen's other keys stay as they are.
    QVariantMap all = choices.value(QStringLiteral("display"), QStringLiteral("screens")).toMap();
    QVariantMap own = all.value(name).toMap();
    if (mode.isEmpty())
    {
        own.remove(QStringLiteral("mode"));
    }
    else
    {
        own.insert(QStringLiteral("mode"), mode);
    }
    all.insert(name, own);
    choices.set(QStringLiteral("display"), QStringLiteral("screens"), all);
}

bool DisplayModes::keepAsked()
{
    QMessageBox question(pane);
    question.setWindowTitle(pane->window()->windowTitle());
    question.setIcon(QMessageBox::Question);
    question.setText(word("display_keep", "Keep this resolution?"));
    const QString countdown =
        word("display_keep_countdown", "The screen goes back to the earlier resolution in %1 seconds.");
    QPushButton* keep = question.addButton(word("display_keep_button", "Keep"), QMessageBox::AcceptRole);
    QPushButton* revert = question.addButton(word("display_revert_button", "Revert"), QMessageBox::RejectRole);
    // A screen that shows nothing leaves Return and Escape to undo it.
    question.setDefaultButton(revert);
    question.setEscapeButton(revert);

    int seconds_left = keep_seconds;
    question.setInformativeText(countdown.arg(seconds_left));
    QTimer tick;
    tick.setInterval(1000);
    connect(&tick, &QTimer::timeout, &question, [&]() {
        seconds_left--;
        question.setInformativeText(countdown.arg(seconds_left));
        if (seconds_left <= 0)
        {
            revert->click();
        }
    });
    tick.start();
    question.exec();
    return question.clickedButton() == keep;
}

QString DisplayModes::word(const char* key, const char* fallback) const
{
    return words.value(QLatin1String(key), QString::fromUtf8(fallback)).toString();
}
