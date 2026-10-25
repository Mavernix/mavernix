// The theme's pane: which of the desktops (the installed ones and those the
// Theme Installer added) is in use and starts when none is named, and a way
// to the Theme Installer for more.

#pragma once

#include <QWidget>

class Choices;

class ThemePane : public QWidget
{
    Q_OBJECT

  public:
    ThemePane(Choices& choices, QWidget* parent = nullptr);
};
