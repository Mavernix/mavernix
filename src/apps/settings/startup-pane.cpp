#include "startup-pane.hpp"

#include <QDialog>
#include <QCoreApplication>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QStandardPaths>
#include <QTimer>
#include <QVBoxLayout>

#include "choices.hpp"
#include "installed-programs.hpp"
#include "pane-parts.hpp"
#include "startup-entries.hpp"

// Short enough that a problem and the note under the buttons fit too.
static const int list_height_px = 200;
static const int icon_size_px = 24;
static const int chooser_width_px = 360;
static const int chooser_height_px = 420;

// The item's file name.
static const int FileNameRole = Qt::UserRole;
static const int UsersOwnRole = Qt::UserRole + 1;

// The arrow keys typed into the search move through the list under it, so
// a program can be found and chosen without leaving the keys.
class KeysToList : public QObject
{
  public:
    KeysToList(QListWidget* list, QObject* parent) : QObject(parent), list(list) {}

  protected:
    bool eventFilter(QObject* watched, QEvent* event) override
    {
        if (event->type() == QEvent::KeyPress)
        {
            const int key = static_cast<QKeyEvent*>(event)->key();
            if (key == Qt::Key_Up || key == Qt::Key_Down || key == Qt::Key_PageUp || key == Qt::Key_PageDown)
            {
                QCoreApplication::sendEvent(list, event);
                return true;
            }
        }
        return QObject::eventFilter(watched, event);
    }

  private:
    QListWidget* list;
};

StartupPane::StartupPane(Choices& choices, QWidget* parent) : QWidget(parent)
{
    words = choices.configTable(QStringLiteral("settings")).value(QStringLiteral("words")).toMap();
    auto* rows = new QVBoxLayout(this);
    // Its parts are as wide as the pane may be; a margin would push them out of the window.
    rows->setContentsMargins(0, 0, 0, 0);
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

    // In the empty list, instead of nothing.
    empty_label = new QLabel(word("startup_none", "No programs open at login."));
    empty_label->setAlignment(Qt::AlignCenter);
    empty_label->setEnabled(false);
    auto* inside_list = new QVBoxLayout(list->viewport());
    inside_list->addWidget(empty_label);

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
    const bool switched_off = users_autostart.isValid() && !users_autostart.toBool();
    if (switched_off)
    {
        note = word("startup_switched_off", "Starting these programs at login is turned off.");
    }
    else if (QStandardPaths::findExecutable(QStringLiteral("dex")).isEmpty())
    {
        note = word("startup_no_dex",
            "These programs don't open yet: the helper program “dex” is missing. Install the package dex, "
            "and they open from the next login.");
    }
    if (!note.isEmpty())
    {
        // As wide as the list above it; with its width fixed, a wrapping
        // label asks for the height it fills.
        auto* note_label = new QLabel(note);
        note_label->setWordWrap(true);
        note_label->setFixedWidth(intro_width_px);
        rows->addWidget(note_label);
    }
    // Turned off in the config: a button turns it back on, rather than a file to edit.
    if (switched_off)
    {
        auto* turn_on = new QPushButton(word("startup_turn_on", "Open Them at Login"));
        connect(turn_on, &QPushButton::clicked, this, [&choices, turn_on]() {
            choices.set(QStringLiteral("session"), QStringLiteral("users_autostart"), true);
            turn_on->setEnabled(false);
        });
        rows->addWidget(turn_on, 0, Qt::AlignLeft);
    }

    connect(list, &QListWidget::itemChanged, this, [this](QListWidgetItem* item) {
        const QString file_name = item->data(FileNameRole).toString();
        showProblem(switch_startup_entry(file_name, item->checkState() == Qt::Checked));
        // The box shows what the folders now say, also when the change
        // failed; read after this signal, as reading clears the list.
        QTimer::singleShot(0, this, [this, file_name]() { fill(file_name); });
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
    empty_label->setVisible(list->count() == 0);
    const QListWidgetItem* current = list->currentItem();
    remove->setEnabled(current != nullptr && current->data(UsersOwnRole).toBool());
}

void StartupPane::addProgram()
{
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

    search->installEventFilter(new KeysToList(programs, &chooser));
    // The first program found is chosen, so Return takes it; one that the
    // search hides is not taken.
    connect(search, &QLineEdit::textChanged, &chooser, [programs](const QString& text) {
        QListWidgetItem* first_found = nullptr;
        for (int row = 0; row < programs->count(); row++)
        {
            QListWidgetItem* item = programs->item(row);
            item->setHidden(!item->text().contains(text, Qt::CaseInsensitive));
            if (first_found == nullptr && !item->isHidden())
            {
                first_found = item;
            }
        }
        if (programs->currentItem() == nullptr || programs->currentItem()->isHidden())
        {
            programs->setCurrentItem(first_found);
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

QString StartupPane::word(const char* key, const char* fallback) const
{
    return words.value(QLatin1String(key), QString::fromUtf8(fallback)).toString();
}

void StartupPane::showProblem(const QString& problem)
{
    problem_label->setText(problem);
    problem_label->setVisible(!problem.isEmpty());
}
