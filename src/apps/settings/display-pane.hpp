// The Display pane. With several screens, a picture of them comes first:
// clicking one picks it, and everything below is for that screen; dragging
// one arranges them ([display.screens.NAME] position), and dragging the menu
// bar moves it and the dock ([display] main_screen).
//
// How large the desktop is drawn is each screen's own: [display.screens.NAME]
// scale, "auto" (2 on a dense screen, else 1) or a number; a screen without
// one follows [display] scale. The theme's pictures are made for 1 and 2 and
// are less sharp between.

#pragma once

#include <QList>
#include <QMap>
#include <QPoint>
#include <QSize>
#include <QString>
#include <QVariantMap>
#include <QWidget>

class Choices;
class DisplayModes;
class QCheckBox;
class QFormLayout;
class QLabel;
class QRadioButton;
class QScreen;
class QSlider;
class QTimer;
class ScreenArrangement;

class DisplayPane : public QWidget
{
    Q_OBJECT

  public:
    DisplayPane(Choices& choices, QWidget* parent = nullptr);

  protected:
    void showEvent(QShowEvent* event) override;

  private:
    // The screens as Qt knows them now: the picture, and the picked one's settings.
    void readScreens();
    void pick(const QString& name);
    // Not called while the slider is dragged: the desktop follows at once,
    // and the window, slider and all, would change size under the pointer.
    void write();
    void showLooksLike();
    // Where the screens stand; after a change of size the session moves
    // them to touch again (make_wayfire_config.py).
    void writePositions(const QMap<QString, QPoint>& positions);
    QScreen* pickedScreen() const;
    QString mainName() const;
    QString word(const char* key, const char* fallback) const;

    Choices& choices;
    QVariantMap words;
    ScreenArrangement* arrangement;
    QLabel* arrangement_note;
    // With one screen: where the arrangement shows up.
    QLabel* one_screen_note;
    QLabel* picked_title;
    QFormLayout* form;
    DisplayModes* modes;
    QString picked;
    // The picked screen's size, in its own pixels.
    QSize screen_pixels;
    // The slider's scales, the largest drawing first.
    QList<double> stops;
    QRadioButton* automatic;
    QRadioButton* chosen;
    QSlider* scale;
    QLabel* looks_like;
    QCheckBox* main_here;
    // Several key steps in a row make one change of the screen.
    QTimer* write_later;
    QTimer* read_later;
    // The backlight's name under /sys/class/backlight; the slider is null without one.
    QString backlight;
    QSlider* brightness = nullptr;
    QWidget* brightness_row = nullptr;
};
