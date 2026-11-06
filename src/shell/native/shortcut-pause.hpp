// Switches the compositor's shortcuts off while `paused` is true, so the
// keys reach the shell's window that has the keyboard: the program switcher
// then takes Tab itself, without a new `qs ipc call` process for each press,
// and holding Tab repeats. Uses the compositor's "shortcut-capture" plugin.
// If another program holds the shortcuts off (Settings recording a shortcut),
// nothing changes. The compositor switches them on again if the shell goes
// away or after the plugin's timeout.
//
// Usage in QML:
//   ShortcutPause { paused: switcher.open && keyTaker.activeFocus }

#pragma once

#include <QObject>

#include "compositor-socket.hpp"

class ShortcutPause : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool paused READ paused WRITE setPaused NOTIFY pausedChanged)

  public:
    explicit ShortcutPause(QObject* parent = nullptr);

    bool paused() const { return wanted; }
    void setPaused(bool paused);

  signals:
    void pausedChanged();

  private:
    void send();

    CompositorSocket compositor;
    bool wanted = false;
    // What was last asked of the compositor, so each start gets one stop.
    bool asked = false;
    // Counts the requests, so a late answer does not undo a newer one.
    int requestNumber = 0;
};
