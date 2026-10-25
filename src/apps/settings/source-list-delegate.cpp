#include "source-list-delegate.hpp"

#include <QApplication>
#include <QPainter>

#include "pane-parts.hpp"

// The list's rows: a row of an
// app is 40 high with a 32 px icon 9 px from the left and its words from 48 px;
// a group's heading row is 22 high with its word 5 px in.
static const int item_row_height_px = 40;
static const int heading_row_height_px = 22;
static const int icon_size_px = 32;
static const int icon_left_px = 9;
static const int words_left_px = 48;
static const int heading_left_px = 5;

void SourceListDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    if (index.data(kind_role).toInt() == GroupHeadingRow)
    {
        QFont heading_font = scaled_font(option.font, small_words_scale);
        heading_font.setBold(true);
        painter->save();
        painter->setFont(heading_font);
        painter->setPen(option.palette.color(QPalette::Disabled, QPalette::Text));
        const QRect words = option.rect.adjusted(heading_left_px, 0, 0, 0);
        painter->drawText(words, Qt::AlignVCenter | Qt::AlignLeft, index.data(Qt::DisplayRole).toString());
        painter->restore();
        return;
    }

    // The background and the chosen row's highlight are the style's: it
    // draws an item view row with no words of its own when none are given.
    QStyleOptionViewItem plain_row = option;
    initStyleOption(&plain_row, index);
    plain_row.text.clear();
    plain_row.icon = QIcon();
    const QWidget* view = option.widget;
    QStyle* style = view ? view->style() : QApplication::style();
    style->drawControl(QStyle::CE_ItemViewItem, &plain_row, painter, view);

    const bool chosen = option.state & QStyle::State_Selected;
    const QPalette::ColorRole text_role = chosen ? QPalette::HighlightedText : QPalette::Text;

    const QIcon icon = index.data(Qt::DecorationRole).value<QIcon>();
    const QRect icon_box(option.rect.left() + icon_left_px,
        option.rect.top() + (option.rect.height() - icon_size_px) / 2, icon_size_px, icon_size_px);
    icon.paint(painter, icon_box);

    // The title and the line under it stand together, in the row's middle;
    // a title with no line under it stands there alone.
    const QString detail = index.data(detail_role).toString();
    const QFont title_font = option.font;
    const QFont detail_font = scaled_font(option.font, small_words_scale);
    const int title_height = QFontMetrics(title_font).height();
    const int detail_height = detail.isEmpty() ? 0 : QFontMetrics(detail_font).height();
    const int words_top = option.rect.top() + (option.rect.height() - title_height - detail_height) / 2;
    const int words_width = option.rect.right() - option.rect.left() - words_left_px;

    painter->save();
    painter->setFont(title_font);
    painter->setPen(option.palette.color(QPalette::Active, text_role));
    painter->drawText(QRect(option.rect.left() + words_left_px, words_top, words_width, title_height),
        Qt::AlignVCenter | Qt::AlignLeft,
        painter->fontMetrics().elidedText(index.data(Qt::DisplayRole).toString(), Qt::ElideRight, words_width));

    if (detail.isEmpty())
    {
        painter->restore();
        return;
    }

    painter->setFont(detail_font);
    // The line under the title is quieter than the title, but on the
    // highlight it stays the highlight's own text colour.
    painter->setPen(option.palette.color(chosen ? QPalette::Active : QPalette::Disabled, text_role));
    painter->drawText(QRect(option.rect.left() + words_left_px, words_top + title_height, words_width, detail_height),
        Qt::AlignVCenter | Qt::AlignLeft, painter->fontMetrics().elidedText(detail, Qt::ElideRight, words_width));
    painter->restore();
}

QSize SourceListDelegate::sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    const int height = index.data(kind_role).toInt() == GroupHeadingRow ? heading_row_height_px : item_row_height_px;
    return QSize(option.rect.width(), height);
}
