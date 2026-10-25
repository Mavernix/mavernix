#include "icon-chooser.hpp"

#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QHash>
#include <QIcon>
#include <QLineEdit>
#include <QListView>
#include <QPushButton>
#include <QSortFilterProxyModel>
#include <QStringListModel>
#include <QVBoxLayout>

#include "icon-theme-names.hpp"

// The names under the icons are elided to the cell's width.
static const int icon_size_px = 48;
static const QSize cell_size(96, 84);
// The grid shows this many columns and rows before it scrolls; the extra
// width is room for the scroll bar.
static const int visible_columns = 6;
static const int visible_rows = 4;
static const int scroll_bar_room_px = 40;
// QDialog's own results are 0 (rejected) and 1 (accepted); the button for
// a picture file closes the window with this one.
static const int file_wanted_result = 2;

static QString word(const QVariantMap& words, const QString& key, const QString& fallback)
{
    return words.value(key, fallback).toString();
}

namespace
{

// A theme has thousands of icons: each is looked up only when its cell is
// drawn, and kept for scrolling back.
class IconNameModel : public QStringListModel
{
  public:
    using QStringListModel::QStringListModel;

    QVariant data(const QModelIndex& index, int role) const override
    {
        if (role == Qt::DecorationRole)
        {
            const QString name = QStringListModel::data(index, Qt::DisplayRole).toString();
            auto known = icons.find(name);
            if (known == icons.end())
            {
                known = icons.insert(name, QIcon::fromTheme(name));
            }
            return known.value();
        }
        if (role == Qt::ToolTipRole)
        {
            return QStringListModel::data(index, Qt::DisplayRole);
        }
        return QStringListModel::data(index, role);
    }

  private:
    mutable QHash<QString, QIcon> icons;
};

} // namespace

IconChooser::IconChooser(QWidget* parent, const QVariantMap& words, const QString& heading) : QDialog(parent)
{
    setWindowTitle(heading);
    setModal(true);

    auto* rows = new QVBoxLayout(this);
    rows->setSpacing(12);

    search = new QLineEdit;
    search->setPlaceholderText(word(words, QStringLiteral("icons_search"), QStringLiteral("Search")));
    search->setClearButtonEnabled(true);
    rows->addWidget(search);

    matching = new QSortFilterProxyModel(this);
    matching->setSourceModel(new IconNameModel(icon_theme_names(), this));
    matching->setFilterCaseSensitivity(Qt::CaseInsensitive);

    grid = new QListView;
    grid->setViewMode(QListView::IconMode);
    grid->setMovement(QListView::Static);
    grid->setResizeMode(QListView::Adjust);
    grid->setUniformItemSizes(true);
    grid->setIconSize(QSize(icon_size_px, icon_size_px));
    grid->setGridSize(cell_size);
    grid->setTextElideMode(Qt::ElideMiddle);
    grid->setMinimumSize(visible_columns * cell_size.width() + scroll_bar_room_px, visible_rows * cell_size.height());
    grid->setModel(matching);
    rows->addWidget(grid);

    auto* file_button =
        new QPushButton(word(words, QStringLiteral("icons_picture_file"), QStringLiteral("Picture File…")));
    buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    buttons->button(QDialogButtonBox::Ok)->setText(word(words, QStringLiteral("ok"), QStringLiteral("OK")));
    buttons->button(QDialogButtonBox::Cancel)->setText(word(words, QStringLiteral("cancel"), QStringLiteral("Cancel")));
    auto* bottom = new QHBoxLayout;
    bottom->addWidget(file_button);
    bottom->addStretch(1);
    bottom->addWidget(buttons);
    rows->addLayout(bottom);

    connect(file_button, &QPushButton::clicked, this, [this]() { done(file_wanted_result); });
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(grid, &QListView::doubleClicked, this, &QDialog::accept);
    connect(search, &QLineEdit::textChanged, this, [this](const QString& text) {
        // Otherwise the view would choose a neighbour of a filtered-away
        // icon, and OK would take an icon nobody clicked.
        const QString chosen = chosenName();
        matching->setFilterFixedString(text.trimmed());
        grid->clearSelection();
        grid->setCurrentIndex(QModelIndex());
        choose(chosen);
    });

    // OK needs an icon chosen; narrowing the list may take the chosen one away.
    auto update_ok = [this]() { buttons->button(QDialogButtonBox::Ok)->setEnabled(!chosenName().isEmpty()); };
    connect(grid->selectionModel(), &QItemSelectionModel::selectionChanged, this, update_ok);
    connect(matching, &QAbstractItemModel::modelReset, this, update_ok);
    connect(matching, &QAbstractItemModel::layoutChanged, this, update_ok);
    connect(matching, &QAbstractItemModel::rowsRemoved, this, update_ok);
    update_ok();

    search->setFocus();
}

void IconChooser::choose(const QString& name)
{
    if (name.isEmpty())
    {
        return;
    }
    const QModelIndexList found =
        matching->match(matching->index(0, 0), Qt::DisplayRole, name, 1, Qt::MatchFixedString | Qt::MatchCaseSensitive);
    if (found.isEmpty())
    {
        return;
    }
    grid->setCurrentIndex(found.first());
    grid->scrollTo(found.first(), QAbstractItemView::PositionAtCenter);
}

QString IconChooser::chosenName() const
{
    const QModelIndexList chosen = grid->selectionModel()->selectedIndexes();
    return chosen.isEmpty() ? QString() : chosen.first().data(Qt::DisplayRole).toString();
}

IconChooser::Outcome IconChooser::ask(QWidget* parent, const QVariantMap& words, const QString& heading,
    const QString& current_name, QString& chosen_name)
{
    IconChooser chooser(parent, words, heading);
    chooser.choose(current_name);
    const int result = chooser.exec();
    if (result == file_wanted_result)
    {
        return FileWanted;
    }
    if (result != QDialog::Accepted)
    {
        return Cancelled;
    }
    chosen_name = chooser.chosenName();
    return chosen_name.isEmpty() ? Cancelled : IconChosen;
}
