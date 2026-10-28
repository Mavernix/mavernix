// The settings of how windows are handled: whether they snap to the
// screen's sides, the key that moves and resizes a window from anywhere on
// it, what a double click on a title bar does, and which side the window
// buttons are on.

#pragma once

#include <QWidget>

class Choices;

class WindowsPane : public QWidget
{
    Q_OBJECT

  public:
    WindowsPane(Choices& choices, QWidget* parent = nullptr);

  private:
    Choices& choices;
};
