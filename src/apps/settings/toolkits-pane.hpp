// The settings of how GTK and Qt programs are drawn: whether each toolkit
// takes the desktop's look, and on which side GTK programs put the window
// buttons they draw in their own title bar.

#pragma once

#include <QWidget>

class Choices;

class ToolkitsPane : public QWidget
{
    Q_OBJECT

  public:
    ToolkitsPane(Choices& choices, QWidget* parent = nullptr);

  private:
    // Also has the session write it where GTK programs read it.
    void changeButtonSide(const QString& side);

    Choices& choices;
};
