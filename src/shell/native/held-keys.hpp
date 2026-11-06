// Tells when a held modifier key is let go, for the program switcher.
// The compositor is asked (its "held-modifiers" plugin) about once a frame
// while `watching`: the shell itself only hears keys while one of its windows
// has the keyboard, and Qt keeps a modifier let go meanwhile as held, even
// after the keyboard comes back. Without the plugin, Qt's view is used.
//
// Usage in QML:
//   HeldKeys {
//       modifier: "super"
//       watching: switcher.open
//       onLetGo: switcher.go()
//   }

#pragma once

#include <QElapsedTimer>
#include <QObject>
#include <QString>
#include <QTimer>

#include "compositor-socket.hpp"

class HeldKeys : public QObject
{
    Q_OBJECT
    // "super", "alt", "ctrl" or "shift".
    Q_PROPERTY(QString modifier READ modifier WRITE setModifier NOTIFY modifierChanged)
    Q_PROPERTY(bool watching READ watching WRITE setWatching NOTIFY watchingChanged)

  public:
    explicit HeldKeys(QObject* parent = nullptr);

    QString modifier() const { return watchedModifier; }
    void setModifier(const QString& modifier);
    bool watching() const { return isWatching; }
    void setWatching(bool watching);

  signals:
    void modifierChanged();
    void watchingChanged();
    // The modifier is not held (any more); once per watch.
    void letGo();

  private:
    void ask();
    void answered(bool held);
    bool heldAsQtSees() const;

    CompositorSocket compositor;
    QTimer nextQuestion;
    QString watchedModifier;
    bool isWatching = false;
    bool letGoSent = false;
    QElapsedTimer sinceWatchStarted;
    // One question at a time; answers to a finished watch are dropped.
    bool waitingForAnswer = false;
    int watchNumber = 0;
    // The compositor has no "held-modifiers" plugin, as found in this watch.
    bool compositorCannotTell = false;
    // Said once, not at every opening of the switcher.
    bool warnedCannotTell = false;
};
