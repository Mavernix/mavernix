// A connection to the compositor's control socket, to switch the desktop's
// shortcuts off while one is recorded (plugin src/compositor/shortcut-capture/).
// They come back when the connection ends, so a crash cannot leave the
// desktop without keys.

#pragma once

#include <QByteArray>
#include <QLocalSocket>
#include <QObject>
#include <QTimer>

class CompositorLink : public QObject
{
    Q_OBJECT

  public:
    enum class Problem
    {
        // Not started from the desktop's session: there is no socket.
        NoSession,
        Unreachable,
        // `detail` is the compositor's error text.
        Refused,
    };
    Q_ENUM(Problem)

    explicit CompositorLink(QObject* parent = nullptr);
    ~CompositorLink() override;

    // Keeps the connection open until stopShortcutCapture().
    void startShortcutCapture();
    void stopShortcutCapture();
    bool isCapturing() const { return capturing; }

  signals:
    void captureStarted();
    void problem(CompositorLink::Problem kind, const QString& detail);

  private:
    void sendRequest(const QString& method);
    void readAnswers();
    void handleAnswer(const QByteArray& json);
    void closeConnection();

    QLocalSocket socket;
    QByteArray received;
    bool capturing = false;
    // The connection closes on the stop request's answer.
    bool stopping = false;
    QTimer stop_timeout;
};
