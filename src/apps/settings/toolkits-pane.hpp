// The settings of how programs are drawn: whether GTK and Qt programs take
// the desktop's look. (The side of the window buttons is in Windows.)

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
