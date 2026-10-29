#include "screen-arrangement.hpp"

#include <QApplication>
#include <QMouseEvent>
#include <QPainter>

#include <algorithm>

#include "screen-layout.hpp"

static const int picture_width_px = 460;
static const int picture_height_px = 170;
static const int margin_px = 12;
// The menu bar's share of a screen's height, and its least height.
static const double menu_bar_share = 0.08;
static const double menu_bar_least_px = 5;
// A screen let go this close to lining up with another's edge lines up with it.
static const int line_up_px = 8;

ScreenArrangement::ScreenArrangement(QWidget* parent) : QWidget(parent)
{
    setCursor(Qt::OpenHandCursor);
}

void ScreenArrangement::setScreens(const QList<ArrangedScreen>& found)
{
    // Not while one is held, which would move under the pointer: kept for when it is let go.
    if (held >= 0)
    {
        waiting = found;
        has_waiting = true;
        return;
    }
    screens = found;
    fit();
    update();
}

void ScreenArrangement::setMain(const QString& name)
{
    main = name;
    update();
}

void ScreenArrangement::setPicked(const QString& name)
{
    picked_name = name;
    update();
}

QSize ScreenArrangement::sizeHint() const
{
    return QSize(picture_width_px, picture_height_px);
}

void ScreenArrangement::fit()
{
    if (screens.isEmpty())
    {
        return;
    }
    QRect all = screens.first().at;
    for (const ArrangedScreen& screen : screens)
    {
        all = all.united(screen.at);
    }
    const double room_width = width() - 2 * margin_px;
    const double room_height = height() - 2 * margin_px;
    ratio = std::min(room_width / std::max(1, all.width()), room_height / std::max(1, all.height()));
    // Centred in the widget.
    offset = QPointF(
        (width() - all.width() * ratio) / 2 - all.x() * ratio, (height() - all.height() * ratio) / 2 - all.y() * ratio);
}

QRectF ScreenArrangement::drawn(const QRect& at) const
{
    return QRectF(at.x() * ratio + offset.x(), at.y() * ratio + offset.y(), at.width() * ratio, at.height() * ratio);
}

QRectF ScreenArrangement::menuBarOf(const QRectF& screen) const
{
    return QRectF(
        screen.x(), screen.y(), screen.width(), std::max(menu_bar_least_px, screen.height() * menu_bar_share));
}

int ScreenArrangement::screenAt(const QPointF& point) const
{
    // The one drawn last is on top.
    for (int index = screens.size() - 1; index >= 0; index--)
    {
        if (drawn(screens[index].at).contains(point))
        {
            return index;
        }
    }
    return -1;
}

void ScreenArrangement::paintEvent(QPaintEvent*)
{
    if (held < 0)
    {
        fit();
    }
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    const QPalette colours = palette();
    for (int index = 0; index < screens.size(); index++)
    {
        const ArrangedScreen& screen = screens[index];
        const QRectF box = drawn(screen.at).adjusted(1, 1, -1, -1);
        const bool is_picked = screen.name == picked_name;
        // Outlined in the text's colour, which stands out in any theme; a highlight may hardly.
        painter.setPen(
            QPen(is_picked ? colours.color(QPalette::WindowText) : colours.color(QPalette::Dark), is_picked ? 2 : 1));
        painter.setBrush(is_picked ? colours.color(QPalette::Highlight) : colours.color(QPalette::Button));
        painter.drawRect(box);

        if (screen.name == main && !holding_menu_bar)
        {
            painter.setPen(QPen(colours.color(QPalette::Dark), 1));
            painter.setBrush(colours.color(QPalette::Light));
            painter.drawRect(menuBarOf(box));
        }

        painter.setPen(colours.color(is_picked ? QPalette::HighlightedText : QPalette::ButtonText));
        const QString label = fontMetrics().elidedText(screen.label, Qt::ElideRight, int(box.width()) - 8);
        painter.drawText(box, Qt::AlignCenter, label);
    }
    // The menu bar on its way to another screen.
    if (holding_menu_bar)
    {
        for (const ArrangedScreen& screen : screens)
        {
            if (screen.name == main)
            {
                QRectF bar = menuBarOf(drawn(screen.at).adjusted(1, 1, -1, -1));
                bar.moveTopLeft(bar.topLeft() + menu_bar_at - pressed_at);
                painter.setPen(QPen(colours.color(QPalette::Dark), 1));
                painter.setBrush(colours.color(QPalette::Light));
                painter.drawRect(bar);
            }
        }
    }
}

void ScreenArrangement::mousePressEvent(QMouseEvent* event)
{
    if (event->button() != Qt::LeftButton)
    {
        return;
    }
    fit();
    held = screenAt(event->position());
    if (held < 0)
    {
        return;
    }
    pressed_at = event->position();
    menu_bar_at = pressed_at;
    held_from = screens[held].at.topLeft();
    dragged = false;
    holding_menu_bar = screens[held].name == main && menuBarOf(drawn(screens[held].at)).contains(pressed_at);
    setCursor(Qt::ClosedHandCursor);
    if (screens[held].name != picked_name)
    {
        picked_name = screens[held].name;
        emit picked(picked_name);
    }
    update();
}

void ScreenArrangement::mouseMoveEvent(QMouseEvent* event)
{
    if (held < 0)
    {
        return;
    }
    const QPointF moved = event->position() - pressed_at;
    if (!dragged && moved.manhattanLength() < QApplication::startDragDistance())
    {
        return;
    }
    dragged = true;
    if (holding_menu_bar)
    {
        menu_bar_at = event->position();
    }
    else
    {
        screens[held].at.moveTopLeft(held_from + (moved / ratio).toPoint());
    }
    update();
}

void ScreenArrangement::mouseReleaseEvent(QMouseEvent* event)
{
    if (held < 0 || event->button() != Qt::LeftButton)
    {
        return;
    }
    int index = held;
    held = -1;
    setCursor(Qt::OpenHandCursor);
    // Screens plugged in or taken out meanwhile: the held one, if still there, keeps where it was dragged.
    if (has_waiting)
    {
        has_waiting = false;
        const ArrangedScreen moved = screens[index];
        screens = waiting;
        index = -1;
        for (int one = 0; one < screens.size(); one++)
        {
            if (screens[one].name == moved.name)
            {
                index = one;
                screens[one].at.moveTopLeft(moved.at.topLeft());
            }
        }
        if (index < 0)
        {
            holding_menu_bar = false;
            fit();
            update();
            return;
        }
    }
    if (holding_menu_bar)
    {
        holding_menu_bar = false;
        const int onto = screenAt(event->position());
        if (dragged && onto >= 0 && screens[onto].name != main)
        {
            main = screens[onto].name;
            emit mainChosen(main);
        }
        update();
        return;
    }
    if (!dragged || screens.size() < 2)
    {
        update();
        return;
    }

    QList<QRect> others;
    for (int other = 0; other < screens.size(); other++)
    {
        if (other != index)
        {
            others << screens[other].at;
        }
    }
    QRect& moved = screens[index].at;
    moved.moveTopLeft(snapped(moved, others, int(line_up_px / ratio)));
    QMap<QString, QRect> all;
    for (const ArrangedScreen& screen : screens)
    {
        all.insert(screen.name, screen.at);
    }
    fit();
    update();
    if (moved.topLeft() != held_from)
    {
        emit arranged(all);
    }
}
