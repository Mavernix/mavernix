#include "held-keys.hpp"

#include <QDebug>
#include <QGuiApplication>
#include <QJsonObject>

namespace
{
// About once a frame; a question takes well under a millisecond.
const int QUESTION_INTERVAL_MS = 16;
// Without the compositor's answer, Qt's view of the keys is only used this
// long after the watch began, when the shell's window has the keyboard and
// so the keys as they are.
const qint64 QT_VIEW_AFTER_MS = 120;
} // namespace

HeldKeys::HeldKeys(QObject* parent) : QObject(parent)
{
    nextQuestion.setSingleShot(true);
    nextQuestion.setInterval(QUESTION_INTERVAL_MS);
    connect(&nextQuestion, &QTimer::timeout, this, &HeldKeys::ask);
    connect(&compositor, &CompositorSocket::connected, this, &HeldKeys::ask);
    // An answer lost with the connection never comes; ask again (Qt's view
    // stands in until the connection is back).
    connect(&compositor, &CompositorSocket::disconnected, this, [this] {
        waitingForAnswer = false;
        nextQuestion.start();
    });
    // Connected before the first watch, so the first question is not late.
    QTimer::singleShot(0, &compositor, &CompositorSocket::open);
}

void HeldKeys::setModifier(const QString& modifier)
{
    if (modifier == watchedModifier)
    {
        return;
    }
    watchedModifier = modifier;
    emit modifierChanged();
}

void HeldKeys::setWatching(bool watching)
{
    if (watching == isWatching)
    {
        return;
    }
    isWatching = watching;
    emit watchingChanged();

    nextQuestion.stop();
    if (watching)
    {
        ++watchNumber;
        letGoSent = false;
        waitingForAnswer = false;
        // Asked again each time: the plugin may have been loaded since, as
        // after an update while the session runs.
        compositorCannotTell = false;
        sinceWatchStarted.start();
        // One try per watch: a failed connection is not retried each frame.
        if (!compositor.isConnected() && !compositor.isConnecting())
        {
            compositor.open();
        }
        ask();
    }
}

void HeldKeys::ask()
{
    if (!isWatching || letGoSent || waitingForAnswer)
    {
        return;
    }
    if (compositorCannotTell || !compositor.isConnected())
    {
        if (sinceWatchStarted.elapsed() < QT_VIEW_AFTER_MS)
        {
            nextQuestion.start();
            return;
        }
        answered(heldAsQtSees());
        return;
    }

    waitingForAnswer = true;
    const int askedIn = watchNumber;
    compositor.request("held-modifiers/get", {}, [this, askedIn](const QJsonObject& answer) {
        if (askedIn != watchNumber)
        {
            return;
        }
        waitingForAnswer = false;
        if (answer.contains("error"))
        {
            if (!warnedCannotTell)
            {
                qWarning().noquote() << "Shell.Native: the compositor cannot tell which keys are held:"
                                     << answer.value("error").toString()
                                     << "(is its \"held-modifiers\" plugin loaded?); the shell's own view is used";
                warnedCannotTell = true;
            }
            compositorCannotTell = true;
            ask();
            return;
        }
        answered(answer.value(watchedModifier).toBool());
    });
}

void HeldKeys::answered(bool held)
{
    if (!isWatching || letGoSent)
    {
        return;
    }
    if (held)
    {
        nextQuestion.start();
        return;
    }
    letGoSent = true;
    emit letGo();
}

bool HeldKeys::heldAsQtSees() const
{
    const Qt::KeyboardModifiers now = QGuiApplication::queryKeyboardModifiers();
    if (watchedModifier == QLatin1String("super"))
        return now.testFlag(Qt::MetaModifier);
    if (watchedModifier == QLatin1String("alt"))
        return now.testFlag(Qt::AltModifier);
    if (watchedModifier == QLatin1String("ctrl"))
        return now.testFlag(Qt::ControlModifier);
    if (watchedModifier == QLatin1String("shift"))
        return now.testFlag(Qt::ShiftModifier);
    return false;
}
