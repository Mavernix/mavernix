#include "theme-pane.hpp"

#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QProcess>
#include <QPushButton>
#include <QTimeEdit>

#include "choices.hpp"
#include "pane-parts.hpp"

// The session's command that makes the running session another desktop, and
// the one that starts at the next login (src/session/bin/mavernix-switch).
static const QString switch_command = QStringLiteral("mavernix-switch");
// The program that makes more looks (src/apps/theme-installer/).
static const QString installer_command = QStringLiteral("mavernix-theme-installer");

// The desktop named in the file that holds the user's choice; empty if
// there is no such file.
static QString chosen_desktop(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
    {
        return QString();
    }

    return QString::fromUtf8(file.readLine()).trimmed();
}

// A desktop's name as people read it: "sea-lion-dark" is "Sea Lion Dark".
static QString readable_name(const QString& name)
{
    QStringList words = name.split(QLatin1Char('-'), Qt::SkipEmptyParts);
    for (QString& word : words)
    {
        word[0] = word[0].toUpper();
    }
    return words.join(QLatin1Char(' '));
}

ThemePane::ThemePane(Choices& choices, QWidget* parent) : QWidget(parent)
{
    const QVariantMap words = choices.configTable(QStringLiteral("settings")).value(QStringLiteral("words")).toMap();
    const auto word = [&words](const char* key, const char* fallback) {
        return words.value(QLatin1String(key), QString::fromUtf8(fallback)).toString();
    };

    const QString choice_file = choices.defaultDesktopFile();
    QString current = chosen_desktop(choice_file);
    if (current.isEmpty())
    {
        current = choices.runningDesktop();
    }

    // Shown by readable name; the item's data is the desktop's own name.
    auto* desktop = new QComboBox;
    for (const QString& name : choices.desktops())
    {
        desktop->addItem(readable_name(name), name);
    }
    desktop->setCurrentIndex(desktop->findData(current));

    // Said under the choice: what changes at once and what does not.
    // Lines broken here: a label that wraps by itself asks for far more
    // height than it fills, and the window grows with it.
    const int note_width = 340;
    auto* note = new QLabel;
    note->setText(in_lines_of(
        word("desktop_note",
            "The desktop changes at once; windows that are open keep their look until they are opened again."),
        note->fontMetrics(), note_width));
    auto* problem = new QLabel;

    // [theme_schedule]: another desktop at night (the shell's ThemeSchedule.qml
    // switches, src/session/theme_schedule.py picks at login).
    const QString schedule = QStringLiteral("theme_schedule");
    const auto scheduled = [&choices, schedule](const char* key, const QVariant& fallback) {
        const QVariant value = choices.value(schedule, QLatin1String(key));
        return value.isValid() ? value : fallback;
    };
    const auto save = [&choices, schedule, problem](const char* key, const QVariant& value) {
        problem->setText(choices.set(schedule, QLatin1String(key), value));
    };

    connect(desktop, &QComboBox::currentIndexChanged, this, [desktop, choice_file, problem, scheduled, save](int index) {
        const QString name = desktop->itemData(index).toString();
        // With the schedule on, a theme picked here other than the night's
        // is the one for the day from now on.
        if (scheduled("enabled", false).toBool() && name != scheduled("night", QString()).toString())
        {
            save("day", name);
        }
        // Inside the session the command does it all, the choice too.
        // Started elsewhere (to try the window), there is no session to
        // change: then the choice is only written down.
        if (QProcess::startDetached(switch_command, {name}))
        {
            problem->clear();
            return;
        }

        QDir().mkpath(QFileInfo(choice_file).path());
        QFile file(choice_file);
        if (file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        {
            file.write(name.toUtf8() + '\n');
            problem->clear();
        }
        else
        {
            problem->setText(QStringLiteral("Could not write %1: %2").arg(choice_file, file.errorString()));
        }
    });

    auto* more = new QPushButton(word("theme_more", "Get More Themes…"));
    more->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    connect(more, &QPushButton::clicked, this, []() { QProcess::startDetached(installer_command, {}); });
    auto* more_note = new QLabel;
    more_note->setText(
        in_lines_of(word("theme_more_note", "The Theme Installer can make more themes, such as Sea Lion, the look of "
                                            "OS X 10.9, from Apple's installer files."),
            more_note->fontMetrics(), note_width));

    auto* form = new QFormLayout(this);
    form->setLabelAlignment(Qt::AlignRight | Qt::AlignTop);
    form->setFormAlignment(Qt::AlignHCenter | Qt::AlignTop);
    form->setVerticalSpacing(14);
    form->addRow(word("theme_choice", "Theme:"), desktop);
    form->addRow(QString(), note);
    form->addRow(QString(), problem);

    auto* at_night = new QCheckBox(word("theme_night", "At night, use"));
    auto* night = new QComboBox;
    // The night's desktop: the one chosen before, else the dark one of the
    // theme in use ("sea-lion-dark" for "sea-lion"), else another one.
    QString night_name = scheduled("night", QString()).toString();
    if (night_name.isEmpty())
    {
        night_name = choices.desktops().contains(current + QStringLiteral("-dark")) ? current + QStringLiteral("-dark")
                                                                                     : QString();
    }
    for (const QString& name : choices.desktops())
    {
        night->addItem(readable_name(name), name);
        if (night_name.isEmpty() && name != current)
        {
            night_name = name;
        }
    }
    night->setCurrentIndex(std::max(0, night->findData(night_name)));
    auto* night_row = new QHBoxLayout;
    night_row->addWidget(at_night);
    night_row->addWidget(night);
    night_row->addStretch();

    const QString time_format = QStringLiteral("HH:mm");
    const auto time_box = [&](const char* key, const char* fallback) {
        auto* box = new QTimeEdit(QTime::fromString(scheduled(key, QString::fromUtf8(fallback)).toString(), time_format));
        box->setDisplayFormat(time_format);
        if (!box->time().isValid())
        {
            box->setTime(QTime::fromString(QString::fromUtf8(fallback), time_format));
        }
        return box;
    };
    auto* night_from = time_box("night_from", "19:00");
    auto* day_from = time_box("day_from", "07:00");
    auto* hours_row = new QHBoxLayout;
    hours_row->setSpacing(6);
    hours_row->addWidget(new QLabel(word("theme_night_from", "from")));
    hours_row->addWidget(night_from);
    hours_row->addWidget(new QLabel(word("theme_night_until", "until")));
    hours_row->addWidget(day_from);
    hours_row->addStretch();

    auto* night_note = new QLabel;
    night_note->setText(in_lines_of(word("theme_night_note",
                                        "A banner says so a few minutes before. A theme you pick in between stays "
                                        "until the next change."),
        night_note->fontMetrics(), note_width));

    const auto show_on = [night, night_from, day_from, night_note](bool on) {
        for (QWidget* part : std::initializer_list<QWidget*>{night, night_from, day_from, night_note})
        {
            part->setEnabled(on);
        }
    };
    const bool on = scheduled("enabled", false).toBool();
    at_night->setChecked(on);
    show_on(on);

    connect(at_night, &QCheckBox::toggled, this, [=](bool on) {
        show_on(on);
        if (on)
        {
            // Everything as shown, and the theme in use now is the day's.
            save("day", desktop->currentData().toString());
            save("night", night->currentData().toString());
            save("night_from", night_from->time().toString(time_format));
            save("day_from", day_from->time().toString(time_format));
        }
        save("enabled", on);
    });
    connect(night, &QComboBox::currentIndexChanged, this,
        [=](int index) { save("night", night->itemData(index).toString()); });
    connect(night_from, &QTimeEdit::timeChanged, this,
        [=](const QTime& time) { save("night_from", time.toString(time_format)); });
    connect(day_from, &QTimeEdit::timeChanged, this,
        [=](const QTime& time) { save("day_from", time.toString(time_format)); });

    form->addRow(QString(), more);
    form->addRow(QString(), more_note);
    form->addRow(word("theme_schedule", "Schedule:"), night_row);
    form->addRow(QString(), hours_row);
    form->addRow(QString(), night_note);
}
