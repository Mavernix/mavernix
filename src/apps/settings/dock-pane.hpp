// The settings of the dock: how large its icons are, whether they grow
// under the pointer and how large they get then.

#pragma once

#include <QWidget>

class Choices;
class QCheckBox;
class QSlider;

class DockPane : public QWidget
{
    Q_OBJECT

  public:
    DockPane(Choices& choices, QWidget* parent = nullptr);

  private:
    // The user's or the config's, else the theme's for this screen.
    int currentIconSize() const;

    Choices& choices;
    QSlider* size = nullptr;
    QCheckBox* magnification = nullptr;
    QSlider* magnified_size = nullptr;
};
