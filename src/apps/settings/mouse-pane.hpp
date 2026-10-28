// The settings of a mouse: which way the wheel scrolls, how fast the
// pointer moves and the wheel scrolls, whether a faster movement takes the
// pointer further, and which button is the primary one.

#pragma once

#include <QWidget>

class Choices;
class QSlider;
class QTimer;

class MousePane : public QWidget
{
    Q_OBJECT

  public:
    MousePane(Choices& choices, QWidget* parent = nullptr);

  private:
    // One of the [input] settings.
    void change(const QString& key, const QVariant& value);
    // Writes both sliders' values.
    void writeSliders();
    // A slider with "Slow" and "Fast" under its ends.
    QWidget* speedSlider(QSlider*& slider, int stops, int at);

    Choices& choices;
    QSlider* speed = nullptr;
    QSlider* scrolling = nullptr;
    QTimer* write_later = nullptr;
    QString slow_word;
    QString fast_word;
};
