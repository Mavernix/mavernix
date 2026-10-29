#include "icons-pane.hpp"

#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QImageReader>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QStandardPaths>
#include <QVBoxLayout>

#include "choices.hpp"
#include "icon-chooser.hpp"
#include "pane-parts.hpp"
#include "source-list-delegate.hpp"

// The list's rows are drawn at 32 (source-list-delegate.cpp).
static const int detail_icon_size_px = 96;
static const int list_icon_size_px = 32;
static const int search_to_list_px = 6;

// Anything not beginning with "/" is a theme icon name, as the shell reads
// it (src/shell/IconSource.qml).
static bool is_file_choice(const QString& choice)
{
    return choice.startsWith(QLatin1Char('/'));
}

static QString label_of_choice(const QString& choice)
{
    return is_file_choice(choice) ? QFileInfo(choice).fileName() : choice;
}

static QIcon icon_of_choice(const QString& choice)
{
    return is_file_choice(choice) ? QIcon(choice) : QIcon::fromTheme(choice);
}

IconsPane::IconsPane(Choices& choices, QWidget* parent) : QWidget(parent), choices(choices)
{
    words = choices.configTable(QStringLiteral("settings")).value(QStringLiteral("words")).toMap();
    applications = installed_applications();

    auto* rows = new QVBoxLayout(this);
    rows->setContentsMargins(0, 0, 0, 0);
    rows->setSpacing(pane_spacing_px);

    auto* intro = new QLabel;
    intro->setText(in_lines_of(
        word("icons_intro", "Give an application another icon: one of the icon theme's, or a picture of your own. It "
                            "is shown instead of the application's own "
                            "wherever the desktop shows applications: the dock, the list of all applications, "
                            "the program switcher and the search."),
        intro->fontMetrics(), intro_width_px));
    rows->addWidget(intro);

    if (applications.isEmpty())
    {
        auto* none = new QLabel(word("icons_none", "No applications are installed."));
        none->setEnabled(false);
        none->setAlignment(Qt::AlignCenter);
        none->setMinimumHeight(detail_box_size.height());
        rows->addWidget(none);
        return;
    }

    auto* side_by_side = new QHBoxLayout;
    side_by_side->setSpacing(list_to_box_px);
    side_by_side->addWidget(buildList(), 0, Qt::AlignVCenter);
    side_by_side->addWidget(buildDetail(), 0, Qt::AlignVCenter);
    rows->addLayout(side_by_side);
    fillList(QString());
}

QString IconsPane::word(const char* key, const char* fallback) const
{
    return words.value(QLatin1String(key), QString::fromUtf8(fallback)).toString();
}

QWidget* IconsPane::buildList()
{
    auto* column = new QWidget;
    auto* rows = new QVBoxLayout(column);
    rows->setContentsMargins(0, 0, 0, 0);
    rows->setSpacing(search_to_list_px);

    search = new QLineEdit;
    search->setPlaceholderText(word("icons_search", "Search"));
    search->setClearButtonEnabled(true);
    rows->addWidget(search);

    list = new QListWidget;
    // Together as high as the Extensions pane's list.
    list->setFixedSize(
        source_list_size.width(), source_list_size.height() - search->sizeHint().height() - search_to_list_px);
    list->setItemDelegate(new SourceListDelegate(list));
    list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    list->setIconSize(QSize(list_icon_size_px, list_icon_size_px));
    rows->addWidget(list);

    connect(list, &QListWidget::currentItemChanged, this, [this](QListWidgetItem* item) {
        // A heading cannot be chosen; the arrow keys step over it.
        if (item && item->data(SourceListDelegate::kind_role).toInt() == SourceListDelegate::ItemRow)
        {
            showApplication(item->data(Qt::UserRole).toString());
        }
    });
    connect(search, &QLineEdit::textChanged, this, [this]() { fillList(shown_id); });
    column->setFixedSize(source_list_size);
    return column;
}

QWidget* IconsPane::buildDetail()
{
    auto* box = new QGroupBox;
    box->setFixedSize(detail_box_size);
    detail = box;
    auto* rows = new QVBoxLayout(box);
    rows->setContentsMargins(box_margin_px, box_margin_px, box_margin_px, box_margin_px);
    rows->setSpacing(pane_spacing_px);

    detail_icon = new QLabel;
    detail_icon->setFixedSize(detail_icon_size_px, detail_icon_size_px);
    rows->addWidget(detail_icon, 0, Qt::AlignHCenter);

    detail_name = new QLabel;
    QFont name_font = detail_name->font();
    name_font.setBold(true);
    detail_name->setFont(name_font);
    detail_name->setAlignment(Qt::AlignHCenter);
    rows->addWidget(detail_name);

    // The ID names the application in the user's file.
    detail_id = new QLabel;
    detail_id->setFont(scaled_font(detail_id->font(), small_words_scale));
    detail_id->setEnabled(false);
    detail_id->setAlignment(Qt::AlignHCenter);
    rows->addWidget(detail_id);

    auto* divider = new QFrame;
    divider->setFrameShape(QFrame::HLine);
    divider->setFrameShadow(QFrame::Sunken);
    rows->addWidget(divider);

    detail_state = new QLabel;
    detail_state->setFont(scaled_font(detail_state->font(), small_words_scale));
    detail_state->setAlignment(Qt::AlignHCenter);
    rows->addWidget(detail_state);

    choose_button = new QPushButton(word("icons_choose", "Choose Icon…"));
    connect(choose_button, &QPushButton::clicked, this, [this]() { askForIcon(); });
    default_button = new QPushButton(word("icons_use_default", "Use the Application's Icon"));
    connect(default_button, &QPushButton::clicked, this, [this]() { changeIcon(shown_id, QString()); });
    auto* buttons = new QHBoxLayout;
    buttons->addStretch();
    buttons->addWidget(choose_button);
    buttons->addWidget(default_button);
    buttons->addStretch();
    rows->addLayout(buttons);

    rows->addStretch(1);
    return box;
}

const InstalledApplication* IconsPane::applicationWithId(const QString& id) const
{
    for (const InstalledApplication& application : applications)
    {
        if (application.id == id)
        {
            return &application;
        }
    }
    return nullptr;
}

QString IconsPane::ownChoiceOf(const QString& id) const
{
    const QString choice = choices.value(QStringLiteral("app_icons"), id).toString();
    if (choice.isEmpty())
    {
        return QString();
    }
    const bool exists = is_file_choice(choice) ? QFileInfo::exists(choice) : QIcon::hasThemeIcon(choice);
    return exists ? choice : QString();
}

QIcon IconsPane::ownIconOf(const InstalledApplication& application) const
{
    // An icon theme without the application's icon gives the plain program
    // icon, as the shell does.
    const QIcon plain_program = QIcon::fromTheme(QStringLiteral("application-x-executable"));
    if (application.icon.startsWith(QLatin1Char('/')))
    {
        return QIcon(application.icon);
    }
    return QIcon::fromTheme(application.icon, plain_program);
}

QIcon IconsPane::iconOf(const InstalledApplication& application) const
{
    const QString choice = ownChoiceOf(application.id);
    return choice.isEmpty() ? ownIconOf(application) : icon_of_choice(choice);
}

void IconsPane::fillList(const QString& chosen_id)
{
    const QString wanted = search->text().trimmed();
    QList<const InstalledApplication*> with_own_icon;
    QList<const InstalledApplication*> others;
    for (const InstalledApplication& application : applications)
    {
        if (!wanted.isEmpty() && !application.name.contains(wanted, Qt::CaseInsensitive) &&
            !application.id.contains(wanted, Qt::CaseInsensitive))
        {
            continue;
        }
        (ownChoiceOf(application.id).isEmpty() ? others : with_own_icon).append(&application);
    }

    // No current-row signals while refilling.
    const QSignalBlocker quiet(list);
    list->clear();
    QListWidgetItem* to_choose = nullptr;
    auto add_group = [&](const QString& heading, const QList<const InstalledApplication*>& group) {
        if (group.isEmpty())
        {
            return;
        }
        auto* heading_item = new QListWidgetItem(heading, list);
        heading_item->setFlags(Qt::NoItemFlags);
        heading_item->setData(SourceListDelegate::kind_role, SourceListDelegate::GroupHeadingRow);
        for (const InstalledApplication* application : group)
        {
            auto* item = new QListWidgetItem(iconOf(*application), application->name, list);
            item->setData(Qt::UserRole, application->id);
            item->setData(SourceListDelegate::kind_role, SourceListDelegate::ItemRow);
            item->setData(SourceListDelegate::detail_role, label_of_choice(ownChoiceOf(application->id)));
            if (!to_choose || application->id == chosen_id)
            {
                to_choose = item;
            }
        }
    };
    add_group(word("icons_group_own", "With an Icon of Your Own"), with_own_icon);
    add_group(word("icons_group_all", "Applications"), others);

    if (to_choose == nullptr)
    {
        showApplication(QString());
        return;
    }
    list->setCurrentItem(to_choose);
    list->scrollToItem(to_choose);
    showApplication(to_choose->data(Qt::UserRole).toString());
}

void IconsPane::showApplication(const QString& id)
{
    shown_id = id;
    const InstalledApplication* application = applicationWithId(id);
    detail->setEnabled(application != nullptr);
    if (application == nullptr)
    {
        detail_icon->clear();
        detail_name->clear();
        detail_id->clear();
        detail_state->clear();
        return;
    }

    detail_icon->setPixmap(
        iconOf(*application).pixmap(QSize(detail_icon_size_px, detail_icon_size_px), devicePixelRatioF()));
    detail_name->setText(application->name);
    detail_id->setText(application->id);

    const QString choice = ownChoiceOf(id);
    QString state;
    if (choice.isEmpty())
    {
        state = word("icons_state_default", "The icon is the application's own.");
    }
    else if (is_file_choice(choice))
    {
        state = word("icons_state_own", "The icon is the picture %1.").arg(label_of_choice(choice));
    }
    else
    {
        state = word("icons_state_theme", "The icon is %1 from the icon theme.").arg(choice);
    }
    detail_state->setText(in_lines_of(state, detail_state->fontMetrics(), box_text_width_px));
    default_button->setEnabled(!choice.isEmpty());
}

void IconsPane::askForIcon()
{
    const InstalledApplication* application = applicationWithId(shown_id);
    if (application == nullptr)
    {
        return;
    }

    // Preselect the icon shown now: the chosen one, else the entry's.
    const QString earlier = ownChoiceOf(shown_id);
    QString current_name = earlier.isEmpty() ? application->icon : earlier;
    if (is_file_choice(current_name))
    {
        current_name.clear();
    }
    const QString heading = word("icons_choose_heading", "Choose an icon for %1").arg(application->name);
    QString chosen_name;
    switch (IconChooser::ask(this, words, heading, current_name, chosen_name))
    {
    case IconChooser::IconChosen:
        changeIcon(shown_id, chosen_name);
        break;
    case IconChooser::FileWanted:
        askForFile();
        break;
    case IconChooser::Cancelled:
        break;
    }
}

void IconsPane::askForFile()
{
    const InstalledApplication* application = applicationWithId(shown_id);
    if (application == nullptr)
    {
        return;
    }

    QStringList patterns;
    for (const QByteArray& format : QImageReader::supportedImageFormats())
    {
        patterns << QStringLiteral("*.") + QString::fromLatin1(format);
    }
    const QString heading = word("icons_choose_heading", "Choose an icon for %1").arg(application->name);

    // Start where the last picture was, else in the user's pictures.
    const QString earlier = ownChoiceOf(shown_id);
    const QString start_folder = !is_file_choice(earlier)
                                     ? QStandardPaths::writableLocation(QStandardPaths::PicturesLocation)
                                     : QFileInfo(earlier).absolutePath();
    const QString file = QFileDialog::getOpenFileName(this, heading, start_folder,
        word("icons_file_kind", "Pictures") + QStringLiteral(" (") + patterns.join(QLatin1Char(' ')) +
            QLatin1Char(')'));
    if (file.isEmpty())
    {
        return;
    }

    // A file the shell cannot show would silently leave the application's
    // own icon, and the user would not know why.
    if (!QImageReader(file).canRead())
    {
        QMessageBox::warning(
            this, heading, word("icons_unreadable", "This file is not a picture that can be shown as an icon."));
        return;
    }
    changeIcon(shown_id, file);
}

void IconsPane::changeIcon(const QString& id, const QString& choice)
{
    // An empty value stays in the user's file; the shell treats it as absent.
    const QString failure = choices.set(QStringLiteral("app_icons"), id, choice);
    if (!failure.isEmpty())
    {
        QMessageBox::warning(this, word("icons_title", "App Icons"), failure);
        return;
    }
    fillList(id);
}
