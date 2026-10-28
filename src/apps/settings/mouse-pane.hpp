// The settings of a mouse: how fast the pointer moves, whether a faster
// movement takes it further, and which way the wheel scrolls.

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
    void writeSpeed();

    Choices& choices;
    QSlider* speed = nullptr;
    QTimer* write_later = nullptr;
};
