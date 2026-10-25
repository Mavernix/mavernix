// How the Extensions and App Icons panes draw their list rows: an icon, a
// title and an optional smaller line, or a small bold group heading.
// Background and highlight are left to the style.

#pragma once

#include <QStyledItemDelegate>

class SourceListDelegate : public QStyledItemDelegate
{
    Q_OBJECT

  public:
    // Kept in Qt::UserRole + 1.
    enum RowKind
    {
        ItemRow,
        GroupHeadingRow,
    };
    static const int kind_role = Qt::UserRole + 1;
    // The smaller line under the title; none if empty.
    static const int detail_role = Qt::UserRole + 2;

    using QStyledItemDelegate::QStyledItemDelegate;

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override;
    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override;
};
