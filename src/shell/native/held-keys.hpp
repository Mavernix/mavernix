// Whether a modifier key is held down right now, for the program switcher:
// QML misses a key released before its window had the keyboard. The state is
// what the compositor last told the shell while it had the keyboard.

#pragma once

#include <QObject>
#include <QString>

class HeldKeys : public QObject
{
    Q_OBJECT

  public:
    // "super", "alt", "ctrl" or "shift".
    Q_INVOKABLE bool held(const QString& modifier) const;
};
