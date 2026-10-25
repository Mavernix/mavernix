// Starts another program for the window's buttons, and lets it live on
// when the window is closed.

#pragma once

#include <QObject>
#include <QStringList>

class Launcher : public QObject
{
    Q_OBJECT

  public:
    // Starts `command` (the program, then its arguments). Says so on the
    // error output if it cannot be started.
    Q_INVOKABLE void run(const QStringList& command) const;
};
