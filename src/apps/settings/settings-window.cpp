#include "settings-window.hpp"

#include <QIcon>
#include <QLineEdit>
#include <QStackedWidget>
#include <QTabWidget>
#include <QToolBar>
#include <QToolButton>
#include <QVBoxLayout>

#include <cstdio>

#include "bluetooth-pane.hpp"
#include "choices.hpp"
#include "desktop-pane.hpp"
#include "desktops-pane.hpp"
#include "defaults-pane.hpp"
#include "dock-pane.hpp"
#include "input-sources-pane.hpp"
#include "keyboard-pane.hpp"
#include "sound-pane.hpp"
#include "startup-pane.hpp"
#include "display-pane.hpp"
#include "energy-pane.hpp"
#include "overview.hpp"
#include "extensions-pane.hpp"
#include "theme-pane.hpp"
#include "icons-pane.hpp"
#include "mouse-pane.hpp"
#include "network-pane.hpp"
#include "trackpad-pane.hpp"
#include "toolkits-pane.hpp"
#include "windows-pane.hpp"

// Each known pane's theme icon, and the title used when the config gives none.
struct KnownPane
{
    const char* name;
    const char* icon;
    const char* title;
};
static const KnownPane known_panes[] = {
    {"dock", "preferences-dock", "Dock"},
    {"icons", "applications-other", "App Icons"},
    {"theme", "preferences-desktop-theme", "Theme"},
    {"desktop", "preferences-desktop-wallpaper", "Desktop"},
    {"sound", "preferences-desktop-sound", "Sound"},
    {"trackpad", "input-touchpad", "Touchpad"},
    {"mouse", "input-mouse", "Mouse"},
    {"energy", "preferences-system-power-management", "Energy"},
    {"display", "preferences-desktop-display", "Display"},
    {"desktops", "preferences-desktop-virtual", "Desktops"},
    {"windows", "preferences-system-windows", "Windows"},
    {"extensions", "preferences-plugin", "Extensions"},
    {"toolkits", "preferences-system", "GTK & Qt"},
    {"network", "preferences-system-network", "Network"},
    {"bluetooth", "preferences-system-bluetooth", "Bluetooth"},
    {"keyboard", "input-keyboard", "Keyboard"},
    {"defaults", "applications-other", "Default Apps"},
    {"startup", "preferences-system-startup", "Startup"},
};

SettingsWindow::SettingsWindow(Choices& choices) : choices(choices)
{
    const QVariantMap wishes = choices.configTable(QStringLiteral("settings"));
    const QVariantMap words = wishes.value(QStringLiteral("words")).toMap();
    program_name = wishes.value(QStringLiteral("name"), QStringLiteral("Settings")).toString();

    auto* tool_bar = addToolBar(QStringLiteral("navigation"));
    tool_bar->setMovable(false);
    tool_bar->setContextMenuPolicy(Qt::PreventContextMenu);

    back_button = new QToolButton;
    back_button->setArrowType(Qt::LeftArrow);
    forward_button = new QToolButton;
    forward_button->setArrowType(Qt::RightArrow);
    connect(back_button, &QToolButton::clicked, this, [this]() { stepInHistory(-1); });
    connect(forward_button, &QToolButton::clicked, this, [this]() { stepInHistory(1); });
    tool_bar->addWidget(back_button);
    tool_bar->addWidget(forward_button);

    // A gap between the pair and "Show All", so that a style which joins
    // neighbouring tool buttons into one control joins only the pair.
    const int gap_between_buttons_px = 8;
    auto* gap = new QWidget;
    gap->setFixedWidth(gap_between_buttons_px);
    tool_bar->addWidget(gap);

    // A tool button: themes draw tool bar buttons differently
    // (`[controls.tool_button]`).
    auto* show_all = new QToolButton;
    show_all->setText(words.value(QStringLiteral("show_all"), QStringLiteral("Show all")).toString());
    show_all->setAutoRaise(false);
    show_all->setFocusPolicy(Qt::TabFocus);
    connect(show_all, &QToolButton::clicked, this, &SettingsWindow::showOverview);
    tool_bar->addWidget(show_all);

    auto* spring = new QWidget;
    spring->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    tool_bar->addWidget(spring);

    search = new QLineEdit;
    search->setPlaceholderText(words.value(QStringLiteral("search"), QStringLiteral("Search")).toString());
    search->setClearButtonEnabled(true);
    const int search_width = 160;
    search->setFixedWidth(search_width);
    tool_bar->addWidget(search);

    pages = new QStackedWidget;
    setCentralWidget(pages);

    // [settings] groups, else one group of [settings] panes, else all known.
    QList<QStringList> wanted_groups;
    for (const QVariant& group : wishes.value(QStringLiteral("groups")).toList())
    {
        wanted_groups.append(group.toStringList());
    }
    if (wanted_groups.isEmpty())
    {
        QStringList wanted = wishes.value(QStringLiteral("panes")).toStringList();
        if (wanted.isEmpty())
        {
            for (const KnownPane& pane : known_panes)
            {
                wanted << QLatin1String(pane.name);
            }
        }
        wanted_groups.append(wanted);
    }

    QList<QList<Overview::Entry>> groups;
    pages->addWidget(new QWidget);
    for (const QStringList& wanted : wanted_groups)
    {
        QList<Overview::Entry> group;
        for (const QString& name : wanted)
        {
            QWidget* pane = makePane(name);
            if (pane == nullptr)
            {
                std::fprintf(stderr,
                    "mavernix-settings: the config asks for a pane \"%s\", "
                    "which this program does not have; known: theme, desktop, dock, icons, sound, trackpad, mouse, energy, "
                    "display, desktops, windows, toolkits, extensions, network, bluetooth, keyboard, defaults\n",
                    qPrintable(name));
                continue;
            }

            const KnownPane* known = nullptr;
            for (const KnownPane& candidate : known_panes)
            {
                if (name == QLatin1String(candidate.name))
                {
                    known = &candidate;
                }
            }
            const QString title = words.value(name + QStringLiteral("_title"), QLatin1String(known->title)).toString();

            auto* page = new QWidget;
            auto* page_rows = new QVBoxLayout(page);
            const int page_margin = 20;
            page_rows->setContentsMargins(page_margin, page_margin, page_margin, page_margin);
            page_rows->addWidget(pane, 0, Qt::AlignHCenter | Qt::AlignTop);
            page_rows->addStretch();
            pane_pages.insert(name, pages->addWidget(page));
            // A title may be broken in two lines for the overview.
            pane_titles.insert(name, QString(title).replace(QLatin1Char('\n'), QLatin1Char(' ')));
            group.append({name, title, QLatin1String(known->icon)});
        }
        groups.append(group);
    }

    overview =
        new Overview(groups, choices.themeTable(QStringLiteral("settings")).value(QStringLiteral("overview")).toMap());
    connect(overview, &Overview::chosen, this, &SettingsWindow::showPane);
    connect(search, &QLineEdit::textChanged, this, [this](const QString& text) {
        if (!text.isEmpty() && pages->currentIndex() != 0)
        {
            showOverview();
        }
        overview->mark(text);
    });
    connect(search, &QLineEdit::returnPressed, this, [this]() {
        const QString found = search->text().isEmpty() ? QString() : overview->firstWith(search->text());
        if (!found.isEmpty())
        {
            search->clear();
            showPane(found);
        }
    });
    // The overview takes the focus itself, so no pane button stands marked
    // before a key is touched; Tab still reaches the buttons.
    overview->setFocusPolicy(Qt::ClickFocus);
    QWidget* placeholder = pages->widget(0);
    pages->removeWidget(placeholder);
    delete placeholder;
    pages->insertWidget(0, overview);

    // Not resizable by hand: it takes each page's own height.
    setFixedWidth(overview->sizeHint().width());
    showOverview();
}

QWidget* SettingsWindow::makePane(const QString& name)
{
    if (name == QLatin1String("dock"))
    {
        return new DockPane(choices);
    }
    if (name == QLatin1String("icons"))
    {
        return new IconsPane(choices);
    }
    if (name == QLatin1String("theme"))
    {
        return new ThemePane(choices);
    }
    if (name == QLatin1String("desktop"))
    {
        return new DesktopPane(choices);
    }
    if (name == QLatin1String("sound"))
    {
        return new SoundPane(choices);
    }
    if (name == QLatin1String("trackpad"))
    {
        return new TrackpadPane(choices);
    }
    if (name == QLatin1String("mouse"))
    {
        return new MousePane(choices);
    }
    if (name == QLatin1String("network"))
    {
        return new NetworkPane(choices);
    }
    if (name == QLatin1String("energy"))
    {
        return new EnergyPane(choices);
    }
    if (name == QLatin1String("display"))
    {
        return new DisplayPane(choices);
    }
    if (name == QLatin1String("desktops"))
    {
        return new DesktopsPane(choices);
    }
    if (name == QLatin1String("windows"))
    {
        return new WindowsPane(choices);
    }
    if (name == QLatin1String("extensions"))
    {
        return new ExtensionsPane(choices);
    }
    if (name == QLatin1String("toolkits"))
    {
        return new ToolkitsPane(choices);
    }
    if (name == QLatin1String("bluetooth"))
    {
        return new BluetoothPane(choices);
    }
    if (name == QLatin1String("keyboard"))
    {
        return makeKeyboardPane();
    }
    if (name == QLatin1String("defaults"))
    {
        return new DefaultsPane(choices);
    }
    if (name == QLatin1String("startup"))
    {
        return new StartupPane(choices);
    }

    return nullptr;
}

// The keyboard's page: tabs for the shortcuts and the input sources.
QWidget* SettingsWindow::makeKeyboardPane()
{
    const QVariantMap words = choices.configTable(QStringLiteral("settings")).value(QStringLiteral("words")).toMap();
    const auto word = [&words](const char* key, const char* fallback) {
        return words.value(QLatin1String(key), QLatin1String(fallback)).toString();
    };

    auto* tabs = new QTabWidget;
    tabs->addTab(new KeyboardPane(choices), word("keyboard_tab_shortcuts", "Shortcuts"));
    tabs->addTab(new InputSourcesPane(choices), word("keyboard_tab_input_sources", "Input Sources"));
    return tabs;
}

void SettingsWindow::showPane(const QString& name)
{
    if (!pane_pages.contains(name))
    {
        return;
    }
    visit(name);
}

void SettingsWindow::showOverview()
{
    visit(QString());
}

void SettingsWindow::visit(const QString& name)
{
    // Forget what lay ahead, as in a browser.
    while (history.size() > history_place + 1)
    {
        history.removeLast();
    }
    if (history.isEmpty() || history.last() != name)
    {
        history.append(name);
    }
    history_place = int(history.size()) - 1;
    showPlace();
}

void SettingsWindow::stepInHistory(int by)
{
    const int place = history_place + by;
    if (place < 0 || place >= history.size())
    {
        return;
    }
    history_place = place;
    showPlace();
}

void SettingsWindow::showPlace()
{
    const QString name = history.value(history_place);
    const bool is_overview = name.isEmpty();
    pages->setCurrentIndex(is_overview ? 0 : pane_pages.value(name));
    setWindowTitle(is_overview ? program_name : pane_titles.value(name));
    // Not while searching: the field keeps the keyboard.
    if (is_overview && !search->hasFocus())
    {
        overview->setFocus();
    }
    back_button->setEnabled(history_place > 0);
    forward_button->setEnabled(history_place + 1 < history.size());

    // The window takes the page's height: every page but the one shown
    // gives up its claim to room, else the highest page would decide.
    for (int index = 0; index < pages->count(); index++)
    {
        const bool shown = index == pages->currentIndex();
        pages->widget(index)->setSizePolicy(
            QSizePolicy::Preferred, shown ? QSizePolicy::Preferred : QSizePolicy::Ignored);
    }
    pages->adjustSize();
    setFixedHeight(sizeHint().height());
}
