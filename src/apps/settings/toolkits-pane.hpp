// The settings of how programs are drawn: the icon themes, for programs and
// for the Dock and menu bar, and whether GTK and Qt programs take the
// desktop's look. (The side of the window buttons is in Windows.)

#pragma once

#include <QStringList>
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

// Rewrites everything that shows icons for the icon themes the user's file
// holds now, programs' and the Dock's and menu bar's, as a switch to the
// running desktop does. False outside a session: then they take effect at
// the next login.
bool apply_icon_theme(const Choices& choices);

// The icon themes the user picked as their file holds them now, programs'
// and the Dock's and menu bar's, each empty for the theme's own: to tell
// whether a reset changed them.
QStringList icon_theme_choices(const Choices& choices);
