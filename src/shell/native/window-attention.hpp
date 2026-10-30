// Which programs have a window asking for the user's attention, through the
// compositor's "window-attention" plugin (X11 programs' urgency hint).
//
// Usage in QML:
//   WindowAttention { id: attention }
//   attention.asks(appId, attention.revision)
//   attention.answered(appId)   once the user has turned to the program

#pragma once

#include <QHash>
#include <QObject>
#include <QString>

#include "compositor-socket.hpp"

class WindowAttention : public QObject
{
    Q_OBJECT
    // Counts the changes, for bindings that call asks().
    Q_PROPERTY(int revision READ revision NOTIFY changed)

  public:
    explicit WindowAttention(QObject* parent = nullptr);

    int revision() const;

    // Whether a window of this program asks. Pass `revision` so a binding
    // updates with it.
    Q_INVOKABLE bool asks(const QString& appId, int revision) const;
    // The program's request is met, as on a Mac once the user turns to it,
    // even if its windows keep asking; a new request counts again.
    Q_INVOKABLE void answered(const QString& appId);

  signals:
    void changed();

  private:
    CompositorSocket compositor;
    // The windows that ask, by their compositor ID, with their app IDs.
    QHash<qint64, QString> asking;
    int changes = 0;
};
