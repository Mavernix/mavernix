// The settings of how programs are drawn: their icon theme, and whether GTK
// and Qt programs take the desktop's look. (The side of the window buttons is
// in Windows.)

#pragma once

#include <QWidget>

class Choices;

class ToolkitsPane : public QWidget
{
    Q_OBJECT

  public:
    ToolkitsPane(Choices& choices, QWidget* parent = nullptr);

  private:
    Choices& choices;
};

// Rewrites everything that shows icons for the icon theme the user's file
// holds now, the Dock and menu bar too, as a switch to the running desktop
// does. False outside a session: then it takes effect at the next login.
bool apply_icon_theme(const Choices& choices);
