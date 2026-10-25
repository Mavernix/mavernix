// The settings of which program opens what: the web browser, the mail
// program, and the programs for folders, text, pictures, documents, video
// and music. A choice is for every kind of file in its group, and is kept
// where all programs look for it, in the user's mimeapps.list.

#pragma once

#include <QWidget>

class Choices;

class DefaultsPane : public QWidget
{
    Q_OBJECT

  public:
    DefaultsPane(Choices& choices, QWidget* parent = nullptr);
};
