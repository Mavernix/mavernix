// The desktop's pane: its picture (picture-chooser.hpp). Which theme is in
// use is the theme's pane (theme-pane.hpp).

#pragma once

#include <QWidget>

class Choices;

class DesktopPane : public QWidget
{
    Q_OBJECT

  public:
    DesktopPane(Choices& choices, QWidget* parent = nullptr);
};
