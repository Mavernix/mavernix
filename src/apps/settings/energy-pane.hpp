// The settings for saving energy (the config's [idle]): when the screen
// switches off, and whether it is locked then and before sleep. Ticked, the
// lock keeps `lock_after_seconds` equal to `screen_off_after_seconds`.

#pragma once

#include <QList>
#include <QVariantMap>
#include <QWidget>

class Choices;
class QCheckBox;
class QLabel;
class QSlider;

class EnergyPane : public QWidget
{
    Q_OBJECT

  public:
    EnergyPane(Choices& choices, QWidget* parent = nullptr);

  private:
    void write();
    // "10 minutes", "Never".
    QString written(int seconds) const;

    Choices& choices;
    QVariantMap words;
    // In seconds; 0 is never.
    QList<int> stops;
    QSlider* screen_off;
    QLabel* screen_off_time;
    QCheckBox* locks;
};
