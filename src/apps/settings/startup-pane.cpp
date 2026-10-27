#include "startup-pane.hpp"

#include <QDialog>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QStandardPaths>
#include <QVBoxLayout>

#include "choices.hpp"
#include "installed-programs.hpp"
#include "pane-parts.hpp"
#include "startup-entries.hpp"

static const int list_height_px = 280;
static const int icon_size_px = 24;
static const int chooser_width_px = 360;
static const int chooser_height_px = 420;

// The item's file name.
static const int FileNameRole = Qt::UserRole;
static const int UsersOwnRole = Qt::UserRole + 1;

StartupPane::StartupPane(Choices& choices, QWidget* parent) : QWidget(parent)
{
    words = choices.configTable(QStringLiteral("settings")).value(QStringLiteral("words")).toMap();
    const auto word = [this](const char* key, const char* fallback) {
        return words.value(QLatin1String(key), QLatin1String(fallback)).toString();
    };

    auto* rows = new QVBoxLayout(this);
    rows->setSpacing(pane_spacing_px);

    auto* intro = new QLabel(word(
        "startup_intro", "These programs open by themselves when you log in. Untick one to keep it from opening."));
    intro->setWordWrap(true);
    intro->setFixedWidth(intro_width_px);
    rows->addWidget(intro);

    list = new QListWidget;
    list->setFixedSize(intro_width_px, list_height_px);
    list->setIconSize(QSize(icon_size_px, icon_size_px));
    rows->addWidget(list);

    auto* add = new QPushButton(word("startup_add", "Add…"));
    remove = new QPushButton(word("startup_remove", "Remove"));
    auto* buttons = new QHBoxLayout;
    buttons->addWidget(add);
    buttons->addWidget(remove);
    buttons->addStretch();
    rows->addLayout(buttons);

    problem_label = new QLabel;
    problem_label->setWordWrap(true);
    problem_label->setFixedWidth(intro_width_px);
    problem_label->hide();
    rows->addWidget(problem_label);

    // Said when nothing in the list would start.
    QString note;
    const QVariant users_autostart = choices.value(QStringLiteral("session"), QStringLiteral("users_autostart"));
    if (users_autostart.isValid() && !users_autostart.toBool())
    {
        note = word("startup_switched_off", "This desktop does not start these programs ([session] users_autostart).");
    }
    else if (QStandardPaths::findExecutable(QStringLiteral("dex")).isEmpty())
    {
        note = word("startup_no_dex", "These programs are not started, because dex is not installed.");
    }
    if (!note.isEmpty())
    {
        auto* note_label = new QLabel(note);
        note_label->setWordWrap(true);
        note_label->setFixedWidth(intro_width_px);
        note_label->setEnabled(false);
        rows->addWidget(note_label);
    }

    connect(list, &QListWidget::itemChanged, this, [this](QListWidgetItem* item) {
        showProblem(switch_startup_entry(item->data(FileNameRole).toString(), item->checkState() == Qt::Checked));
    });
    connect(list, &QListWidget::currentItemChanged, this, [this](QListWidgetItem* current) {
        // One only the system has is switched off instead.
        remove->setEnabled(current != nullptr && current->data(UsersOwnRole).toBool());
    });
    connect(add, &QPushButton::clicked, this, &StartupPane::addProgram);
    connect(remove, &QPushButton::clicked, this, [this]() {
        const QListWidgetItem* current = list->currentItem();
        if (current != nullptr)
        {
            showProblem(remove_startup_entry(current->data(FileNameRole).toString()));
            fill();
        }
    });

    fill();
}

void StartupPane::fill(const QString& chosen)
{
    const QSignalBlocker quiet(list);
    list->clear();
    for (const StartupEntry& entry : startup_entries())
    {
        auto* item = new QListWidgetItem(entry.icon, entry.name, list);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(entry.on ? Qt::Checked : Qt::Unchecked);
        item->setData(FileNameRole, entry.file_name);
        item->setData(UsersOwnRole, entry.users_own);
        if (entry.file_name == chosen)
        {
            list->setCurrentItem(item);
        }
    }
    const QListWidgetItem* current = list->currentItem();
    remove->setEnabled(current != nullptr && current->data(UsersOwnRole).toBool());
}

void StartupPane::addProgram()
{
    const auto word = [this](const char* key, const char* fallback) {
        return words.value(QLatin1String(key), QLatin1String(fallback)).toString();
    };

    QDialog chooser(this);
    chooser.setWindowTitle(word("startup_add_heading", "Open at Login"));
    chooser.resize(chooser_width_px, chooser_height_px);

    auto* search = new QLineEdit;
    search->setPlaceholderText(word("startup_search", "Search"));
    search->setClearButtonEnabled(true);
    auto* programs = new QListWidget;
    programs->setIconSize(QSize(icon_size_px, icon_size_px));
    for (const InstalledProgram& program : installed_programs())
    {
        auto* item = new QListWidgetItem(program.icon, program.name, programs);
        item->setData(FileNameRole, program.id);
    }
    auto* answer = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    answer->button(QDialogButtonBox::Ok)->setEnabled(false);

    auto* rows = new QVBoxLayout(&chooser);
    rows->addWidget(search);
    rows->addWidget(programs);
    rows->addWidget(answer);

    connect(search, &QLineEdit::textChanged, &chooser, [programs](const QString& text) {
        for (int row = 0; row < programs->count(); row++)
        {
            QListWidgetItem* item = programs->item(row);
            item->setHidden(!item->text().contains(text, Qt::CaseInsensitive));
        }
    });
    connect(programs, &QListWidget::currentItemChanged, &chooser,
        [answer](QListWidgetItem* current) { answer->button(QDialogButtonBox::Ok)->setEnabled(current != nullptr); });
    connect(programs, &QListWidget::itemActivated, &chooser, &QDialog::accept);
    connect(answer, &QDialogButtonBox::accepted, &chooser, &QDialog::accept);
    connect(answer, &QDialogButtonBox::rejected, &chooser, &QDialog::reject);

    if (chooser.exec() != QDialog::Accepted || programs->currentItem() == nullptr)
    {
        return;
    }
    const QString id = programs->currentItem()->data(FileNameRole).toString();
    showProblem(add_startup_program(id));
    fill(id);
}

void StartupPane::showProblem(const QString& problem)
{
    problem_label->setText(problem);
    problem_label->setVisible(!problem.isEmpty());
}
