// The Display pane's picture of the screens, laid out as the desktop has
// them, each drawn to scale with its name. Clicking one picks it for the
// settings below; dragging one moves it, and when let go it lands beside
// another (screen-layout.hpp). The main screen carries a bar for the menu
// bar, which can be dragged onto another screen to move the menu bar and
// dock there.

#pragma once

#include <QList>
#include <QMap>
#include <QPoint>
#include <QRect>
#include <QString>
#include <QWidget>

struct ArrangedScreen
{
    QString name;
    QString label;
    // On the desktop, in logical pixels.
    QRect at;
};

class ScreenArrangement : public QWidget
{
    Q_OBJECT

  public:
    explicit ScreenArrangement(QWidget* parent = nullptr);

    void setScreens(const QList<ArrangedScreen>& screens);
    void setMain(const QString& name);
    void setPicked(const QString& name);

    QSize sizeHint() const override;

  signals:
    void picked(const QString& name);
    // A screen was let go: where every screen now stands.
    void arranged(const QMap<QString, QRect>& screens);
    void mainChosen(const QString& name);

  protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

  private:
    // From the desktop's pixels to the widget's and back. The fit is worked
    // out once per press, so a screen being dragged does not rescale the rest.
    void fit();
    QRectF drawn(const QRect& at) const;
    QRectF menuBarOf(const QRectF& screen) const;
    int screenAt(const QPointF& point) const;

    QList<ArrangedScreen> screens;
    QString main;
    QString picked_name;
    double ratio = 1;
    QPointF offset;

    // While the pointer is down: the screen held, or the menu bar.
    int held = -1;
    bool holding_menu_bar = false;
    bool dragged = false;
    QPointF pressed_at;
    QPoint held_from;
    QPointF menu_bar_at;
};
