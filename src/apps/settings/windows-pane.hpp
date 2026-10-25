// The settings of how windows are handled with the pointer: whether they
// snap to the screen's sides, and the key that moves and resizes a window
// from anywhere on it.

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
