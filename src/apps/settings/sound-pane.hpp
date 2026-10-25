// The settings of sound: which loudspeaker and microphone are used, whether
// the desktop's own sounds are played, and from which sound pack.

#pragma once

#include <QWidget>

class Choices;

class SoundPane : public QWidget
{
    Q_OBJECT

  public:
    SoundPane(Choices& choices, QWidget* parent = nullptr);

  private:
    // The data folders' sounds/NAME that hold a stereo/ folder.
    static QStringList packs();
    static QStringList alertsOf(const QString& pack);
};
