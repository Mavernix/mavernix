// The desktop's pane: its picture (picture-chooser.hpp) and the name the menu
// bar shows while no window is active. Which theme is in use is the theme's
// pane (theme-pane.hpp).

#pragma once

#include <QWidget>

class Choices;

class DesktopPane : public QWidget
{
    Q_OBJECT

  public:
    DesktopPane(Choices& choices, QWidget* parent = nullptr);
};
