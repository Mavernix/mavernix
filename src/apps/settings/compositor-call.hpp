// One question to the compositor's control socket (Wayfire's "ipc" plugin)
// and its answer. A message is the length of its JSON as 4 bytes, least
// significant first, and then the JSON (src/session/compositor_call.py).

#pragma once

#include <QByteArray>
#include <QJsonObject>
#include <QString>

#include <functional>

class QObject;

QByteArray compositor_message(const QString& method, const QJsonObject& data = QJsonObject());

// Calls `answered` once: with the answer, or with an empty answer and what
// went wrong (no session, no answer in time, the compositor's error). It is
// not called if `context` is gone first.
void ask_compositor(const QString& method, QObject* context,
    std::function<void(const QJsonObject& answer, const QString& problem)> answered);
