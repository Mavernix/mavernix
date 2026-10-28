#include "overview.hpp"

#include <QAbstractButton>
#include <QColor>
#include <QIcon>
#include <QPainter>
#include <QStyle>
#include <QStyleOptionFocusRect>

static int number(const QVariantMap& looks, const char* key, int otherwise)
{
    return looks.value(QLatin1String(key), otherwise).toInt();
}

static QColor color(const QVariantMap& looks, const char* key, const char* otherwise)
{
    return QColor(looks.value(QLatin1String(key), QLatin1String(otherwise)).toString());
}

// Opacity of a pane's button that does not match the search.
static const double paled_opacity = 0.3;
// Opacity of black over a pressed icon.
static const double pressed_darkening = 0.35;

struct CellLooks
{
    int icon_size;
    int icon_top;
    int label_baseline;
    int label_line_pitch;
    QColor label_color;
};

// One pane's button. Drawn here rather than as a tool button, whose style
// pads and centres as it likes: the theme says where icon and words lie.
class OverviewCell : public QAbstractButton
{
  public:
    OverviewCell(const QIcon& icon, const QStringList& lines, const CellLooks& looks, QWidget* parent)
        : QAbstractButton(parent), lines(lines), looks(looks)
    {
        setIcon(icon);
        setFocusPolicy(Qt::TabFocus);
    }

    void setStrength(double strength)
    {
        this->strength = strength;
        update();
    }

  protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter painter(this);
        painter.setOpacity(strength);

        const int icon_left = (width() - looks.icon_size) / 2;
        // Icon themes seldom have the exact size, and an enlarged smaller
        // one is blurred: take twice the size and scale down.
        const int taken_size = looks.icon_size * 2;
        QPixmap picture = icon().pixmap(QSize(taken_size, taken_size), devicePixelRatioF());
        painter.setRenderHint(QPainter::SmoothPixmapTransform);
        if (isDown())
        {
            // Darkened only where the icon is, not its whole square.
            QPainter darkener(&picture);
            darkener.setCompositionMode(QPainter::CompositionMode_SourceAtop);
            darkener.fillRect(picture.rect(), QColor(0, 0, 0, int(pressed_darkening * 255)));
        }
        painter.drawPixmap(QRect(icon_left, looks.icon_top, looks.icon_size, looks.icon_size), picture);

        painter.setPen(looks.label_color);
        for (int line = 0; line < lines.size(); line++)
        {
            const int text_width = fontMetrics().horizontalAdvance(lines[line]);
            painter.drawText(
                (width() - text_width) / 2, looks.label_baseline + line * looks.label_line_pitch, lines[line]);
        }

        if (hasFocus())
        {
            QStyleOptionFocusRect ring;
            ring.initFrom(this);
            style()->drawPrimitive(QStyle::PE_FrameFocusRect, &ring, &painter, this);
        }
    }

  private:
    QStringList lines;
    CellLooks looks;
    double strength = 1.0;
};

// As the config breaks it, else one line, or two broken at the space
// nearest the middle if it is wider than its cell.
static QStringList in_lines(const QString& title, const QFontMetrics& metrics, int widest)
{
    if (title.contains(QLatin1Char('\n')))
    {
        return title.split(QLatin1Char('\n'));
    }
    if (metrics.horizontalAdvance(title) <= widest)
    {
        return {title};
    }
    int best = -1;
    for (int at = title.indexOf(QLatin1Char(' ')); at >= 0; at = title.indexOf(QLatin1Char(' '), at + 1))
    {
        if (best < 0 || qAbs(at - title.size() / 2) < qAbs(best - title.size() / 2))
        {
            best = at;
        }
    }
    if (best < 0)
    {
        return {title};
    }
    return {title.left(best), title.mid(best + 1)};
}

Overview::Overview(const QList<QList<Entry>>& groups, const QVariantMap& looks, QWidget* parent)
    : QWidget(parent), looks(looks)
{
    const int columns = number(looks, "columns", 6);
    const int cell_pitch = number(looks, "cell_pitch", 110);
    const int first_cell_middle = number(looks, "first_cell_middle", 65);
    const int line_pitch = number(looks, "line_pitch", 104);
    const int group_bottom = number(looks, "group_bottom", 12);
    // A line between two stripes, and a lighter one under it.
    const int lines_between = 2;

    CellLooks cell;
    cell.icon_size = number(looks, "icon_size", 48);
    cell.icon_top = number(looks, "icon_top", 14);
    cell.label_baseline = number(looks, "label_baseline", 82);
    cell.label_line_pitch = number(looks, "label_line_pitch", 15);
    cell.label_color = color(looks, "label_color", "#202020");

    total_width = first_cell_middle + (columns - 1) * cell_pitch + first_cell_middle;

    QFont label_font = font();
    label_font.setPixelSize(number(looks, "label_font_size", 12));
    // A little wider than the pitch, as two neighbours seldom both are.
    const int widest_label = cell_pitch - 4;

    int top = 0;
    for (const QList<Entry>& group : groups)
    {
        if (group.isEmpty())
        {
            continue;
        }
        const int lines = (int(group.size()) + columns - 1) / columns;
        const int height = lines * line_pitch + group_bottom;
        stripes.append({top, height});

        for (int place = 0; place < group.size(); place++)
        {
            const Entry& entry = group[place];
            const QIcon icon = QIcon::fromTheme(entry.icon, QIcon::fromTheme(QStringLiteral("preferences-system")));
            auto* button =
                new OverviewCell(icon, in_lines(entry.title, QFontMetrics(label_font), widest_label), cell, this);
            button->setFont(label_font);
            button->setAccessibleName(QString(entry.title).replace(QLatin1Char('\n'), QLatin1Char(' ')));
            // A cell reaches into the room under its line, for a name's second line.
            button->setGeometry(first_cell_middle - cell_pitch / 2 + (place % columns) * cell_pitch,
                top + (place / columns) * line_pitch, cell_pitch, line_pitch + group_bottom);
            connect(button, &QAbstractButton::clicked, this, [this, name = entry.name]() { emit chosen(name); });
            buttons.append(
                {button, entry.name, QString(entry.title).replace(QLatin1Char('\n'), QLatin1Char(' ')), entry.keywords});
        }
        top += height + lines_between;
    }
    total_height = top - lines_between;
}

QSize Overview::sizeHint() const
{
    return QSize(total_width, total_height);
}

void Overview::mark(const QString& text)
{
    for (const Button& entry : buttons)
    {
        const bool paled = !text.isEmpty() && !entry.matches(text);
        entry.button->setStrength(paled ? paled_opacity : 1.0);
    }
}

QString Overview::firstWith(const QString& text) const
{
    for (const Button& entry : buttons)
    {
        if (entry.matches(text))
        {
            return entry.name;
        }
    }
    return QString();
}

void Overview::paintEvent(QPaintEvent*)
{
    QPainter painter(this);
    const QColor shades[] = {color(looks, "stripe", "#f4f4f4"), color(looks, "stripe_alternate", "#ebebeb")};
    // The lines under a stripe have their own colours for each shade.
    const QStringList lines = looks.value(QStringLiteral("line_colors")).toStringList();
    const QStringList light_lines = looks.value(QStringLiteral("light_line_colors")).toStringList();
    const auto line_color = [](const QStringList& colors, int index, const char* otherwise) {
        return colors.isEmpty() ? QColor(QLatin1String(otherwise)) : QColor(colors[index % colors.size()]);
    };

    for (int index = 0; index < stripes.size(); index++)
    {
        const Stripe& stripe = stripes[index];
        painter.fillRect(0, stripe.top, width(), stripe.height, shades[index % 2]);
        painter.fillRect(0, stripe.top + stripe.height, width(), 1, line_color(lines, index, "#d0d0d0"));
        painter.fillRect(0, stripe.top + stripe.height + 1, width(), 1, line_color(light_lines, index, "#f8f8f8"));
    }
    // Below the last stripe the first shade goes on to the window's end.
    if (!stripes.isEmpty())
    {
        const int below = stripes.last().top + stripes.last().height + 2;
        painter.fillRect(0, below, width(), height() - below, shades[stripes.size() % 2]);
    }
}
