// The settings for saving energy (the config's [idle]): when the screen
// switches off, when the machine goes to sleep, and whether the screen is
// locked then and before sleep. Ticked, the lock keeps `lock_after_seconds`
// equal to `screen_off_after_seconds`.

#pragma once

#include <QList>
#include <QVariantMap>
#include <QWidget>

class Choices;
class QCheckBox;
class QHBoxLayout;
class QLabel;
class QSlider;
class QTimer;

class EnergyPane : public QWidget
{
    Q_OBJECT

  public:
    EnergyPane(Choices& choices, QWidget* parent = nullptr);
    ~EnergyPane() override;

  private:
    // A slider over `stops` standing at the one nearest the config's key,
    // with the time it stands for beside it.
    QHBoxLayout* timeSlider(const QString& key, QSlider*& slider);
    void write();
    // "10 minutes", "Never".
    QString written(int seconds) const;

    Choices& choices;
    QVariantMap words;
    // In seconds; 0 is never.
    QList<int> stops;
    QSlider* screen_off = nullptr;
    QSlider* sleep = nullptr;
    QCheckBox* locks;
    QTimer* write_later;
};
