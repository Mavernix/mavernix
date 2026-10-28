// How large the desktop is drawn on the screen: the config's [display]
// scale, "auto" (2 on a dense screen, else 1) or a number. The theme's
// pictures are made for 1 and 2 and are less sharp between.

#pragma once

#include <QList>
#include <QSize>
#include <QString>
#include <QVariantMap>
#include <QWidget>

class Choices;
class QLabel;
class QRadioButton;
class QSlider;
class QTimer;

class DisplayPane : public QWidget
{
    Q_OBJECT

  public:
    DisplayPane(Choices& choices, QWidget* parent = nullptr);

  protected:
    void showEvent(QShowEvent* event) override;

  private:
    // Not called while the slider is dragged: the desktop follows at once,
    // and the window, slider and all, would change size under the pointer.
    void write();
    void showLooksLike();

    Choices& choices;
    QVariantMap words;
    // As it was when the pane opened.
    QSize screen_pixels;
    // The slider's scales, the largest drawing first.
    QList<double> stops;
    QRadioButton* automatic;
    QRadioButton* chosen;
    QSlider* scale;
    QLabel* looks_like;
    // Several key steps in a row make one change of the screen.
    QTimer* write_later;
    // The backlight's name under /sys/class/backlight; the slider is null without one.
    QString backlight;
    QSlider* brightness = nullptr;
};
