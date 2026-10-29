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
    void writePositions(const QMap<QString, QPoint>& positions);
    // After a change of scale or resolution the picked screen is wider or
    // narrower, so the others are moved to touch it again.
    void tidyLater();
    QScreen* pickedScreen() const;
    QString mainName() const;
    QString word(const char* key, const char* fallback) const;

    Choices& choices;
    QVariantMap words;
    ScreenArrangement* arrangement;
    QLabel* arrangement_note;
    QLabel* picked_title;
    QFormLayout* form;
    DisplayModes* modes;
    QString picked;
    // The picked screen's, in its own pixels.
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
    // Screens plugged in, taken out or resized come in bursts.
    QTimer* read_later;
    bool tidy_pending = false;
    // The backlight's name under /sys/class/backlight; the slider is null without one.
    QString backlight;
    QSlider* brightness = nullptr;
    QWidget* brightness_row = nullptr;
};
