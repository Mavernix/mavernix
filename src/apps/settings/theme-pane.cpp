#include "theme-pane.hpp"

#include <QComboBox>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFormLayout>
#include <QLabel>
#include <QProcess>
#include <QPushButton>

#include "choices.hpp"

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
        return words.value(QLatin1String(key), QLatin1String(fallback)).toString();
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
    auto* note = new QLabel(word("desktop_note",
        "The desktop changes at once; windows that are open keep their look until they are opened again."));
    note->setWordWrap(true);
    auto* problem = new QLabel;
    problem->setWordWrap(true);

    connect(desktop, &QComboBox::currentIndexChanged, this, [desktop, choice_file, problem](int index) {
        const QString name = desktop->itemData(index).toString();
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
    auto* more_note =
        new QLabel(word("theme_more_note", "The Theme Installer makes more themes from a Mac installer."));
    more_note->setWordWrap(true);

    auto* form = new QFormLayout(this);
    form->setLabelAlignment(Qt::AlignRight | Qt::AlignTop);
    form->setFormAlignment(Qt::AlignHCenter | Qt::AlignTop);
    form->setVerticalSpacing(14);
    form->addRow(word("theme_choice", "Theme:"), desktop);
    form->addRow(QString(), note);
    form->addRow(QString(), problem);
    form->addRow(QString(), more);
    form->addRow(QString(), more_note);
}
