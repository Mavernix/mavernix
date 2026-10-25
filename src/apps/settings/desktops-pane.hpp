// The settings of the desktops that lie side by side: how many there are,
// whether fingers on a touchpad pull the next one in, and whether a
// window held at the screen's side goes to the next one.

#pragma once

#include <QWidget>

class Choices;

class DesktopsPane : public QWidget
{
    Q_OBJECT

  public:
    DesktopsPane(Choices& choices, QWidget* parent = nullptr);

  private:
    Choices& choices;
};
