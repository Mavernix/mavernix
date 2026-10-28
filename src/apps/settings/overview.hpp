// The settings window's first page: every pane as an icon with its name,
// in stripes of alternating shades. Sizes and colours are the theme's
// [settings.overview]; the groups are the config's [settings] groups.

#pragma once

#include <QList>
#include <QString>
#include <QVariantMap>
#include <QWidget>

class OverviewCell;

class Overview : public QWidget
{
    Q_OBJECT

  public:
    struct Entry
    {
        QString name;
        QString title;
        QString icon;
        // Space-separated words that also find it.
        QString keywords;
    };

    Overview(const QList<QList<Entry>>& groups, const QVariantMap& looks, QWidget* parent = nullptr);

    QSize sizeHint() const override;

    // Pales the panes whose title does not hold `text`.
    void mark(const QString& text);
    // Empty if none does.
    QString firstWith(const QString& text) const;

  signals:
    void chosen(const QString& name);

  protected:
    void paintEvent(QPaintEvent* event) override;

  private:
    struct Stripe
    {
        int top;
        int height;
    };
    QList<Stripe> stripes;
    struct Button
    {
        OverviewCell* button;
        QString name;
        QString title;
        QString keywords;

        // Whether a search for `text` finds it, by its title or a keyword.
        bool matches(const QString& text) const
        {
            return title.contains(text, Qt::CaseInsensitive) || keywords.contains(text, Qt::CaseInsensitive);
        }
    };
    QList<Button> buttons;

    QVariantMap looks;
    int total_width = 0;
    int total_height = 0;
};
