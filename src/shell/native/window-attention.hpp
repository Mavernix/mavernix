// Which programs have a window asking for the user's attention, through the
// compositor's "window-attention" plugin (X11 programs' urgency hint).
//
// Usage in QML:
//   WindowAttention { id: attention }
//   attention.appIds.includes(appId)

#pragma once

#include <QHash>
#include <QObject>
#include <QStringList>

#include "compositor-socket.hpp"

class WindowAttention : public QObject
{
    Q_OBJECT
    // The app IDs of the programs with a window that asks.
    Q_PROPERTY(QStringList appIds READ appIds NOTIFY changed)

  public:
    explicit WindowAttention(QObject* parent = nullptr);

    QStringList appIds() const;

  signals:
    void changed();

  private:
    CompositorSocket compositor;
    // The windows that ask, by their compositor ID, with their app IDs.
    QHash<qint64, QString> asking;
};
