// The settings of the touchpad: whether a tap is a click, and which way
// two fingers scroll.

#pragma once

#include <QWidget>

class Choices;

class TrackpadPane : public QWidget
{
    Q_OBJECT

  public:
    TrackpadPane(Choices& choices, QWidget* parent = nullptr);

  private:
    // One of the [input] settings.
    void change(const QString& key, bool on);

    Choices& choices;
};
