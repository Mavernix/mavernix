#include "shortcut-recorder.hpp"

#include <QEvent>
#include <QFocusEvent>
#include <QKeyEvent>

#include <linux/input-event-codes.h>

#include "shortcut-editor.hpp"

static QString word(const QVariantMap& words, const QString& key, const QString& fallback)
{
    return words.value(key, fallback).toString();
}

ShortcutRecorder::ShortcutRecorder(const QVariantMap& words, QWidget* parent) : QPushButton(parent), words(words)
{
    setFocusPolicy(Qt::StrongFocus);
    // Wide enough for the longest shortcut, so that the window does not
    // change its size while keys are held.
    const int recorder_width_px = 300;
    setMinimumWidth(recorder_width_px);

    connect(this, &QPushButton::clicked, this, [this]() {
        if (!recording)
        {
            startRecording();
        }
    });
    connect(&link, &CompositorLink::problem, this, &ShortcutRecorder::onLinkProblem);
    connect(&link, &CompositorLink::captureStarted, this, [this]() { emit note(QString()); });
    showIdle();
}

void ShortcutRecorder::startRecording()
{
    if (recording)
    {
        return;
    }
    recording = true;
    held_modifiers.clear();
    held_modifier_codes.clear();
    emit note(QString());
    showHeld();
    // Recording also works without the compositor's help, for the keys it
    // does not take.
    link.startShortcutCapture();
}

void ShortcutRecorder::stopRecording()
{
    if (!recording)
    {
        return;
    }
    recording = false;
    link.stopShortcutCapture();
    showIdle();
}

void ShortcutRecorder::showIdle()
{
    setText(word(words, QStringLiteral("shortcut_record_idle"), QStringLiteral("Click here, then press the keys")));
}

void ShortcutRecorder::showHeld()
{
    const QStringList held = held_modifiers.values();
    const QString shown = modifiers_text(held, words);
    setText(shown.isEmpty()
                ? word(words, QStringLiteral("shortcut_record_waiting"), QStringLiteral("Press the new keys…"))
                : shown);
}

void ShortcutRecorder::onLinkProblem(CompositorLink::Problem kind, const QString& detail)
{
    // Recording goes on; the list below the button can always be used.
    switch (kind)
    {
    case CompositorLink::Problem::NoSession:
        emit note(word(words, QStringLiteral("shortcut_record_no_session"),
            QStringLiteral("This window is not running in the desktop, so the desktop's own keys cannot be "
                           "recorded here. Choose them from the list below.")));
        break;
    case CompositorLink::Problem::Unreachable:
        emit note(word(words, QStringLiteral("shortcut_record_unreachable"),
            QStringLiteral("The desktop could not be reached, so its own keys may not be recorded. Choose "
                           "them from the list below. (%1)"))
                .arg(detail));
        break;
    case CompositorLink::Problem::Refused:
        emit note(word(words, QStringLiteral("shortcut_record_refused"),
            QStringLiteral("The desktop refused to hand over its keys (%1). Choose them from the list below."))
                .arg(detail));
        break;
    }
}

void ShortcutRecorder::handleKey(bool pressed, quint32 scan_code, bool auto_repeat)
{
    if (!recording)
    {
        return;
    }
    const int code = int(scan_code) - scan_code_offset;

    const QString modifier = modifier_from_code(code);
    if (!modifier.isEmpty())
    {
        if (pressed)
        {
            held_modifier_codes.insert(code);
        }
        else
        {
            held_modifier_codes.remove(code);
        }
        // A modifier is held while either of its keys is down.
        held_modifiers.clear();
        for (const int held_code : held_modifier_codes)
        {
            held_modifiers.insert(modifier_from_code(held_code));
        }
        showHeld();
        return;
    }

    if (!pressed || auto_repeat)
    {
        return;
    }

    // Escape on its own is "never mind". With a modifier it is a key like
    // another.
    if (code == KEY_ESC && held_modifiers.isEmpty())
    {
        stopRecording();
        return;
    }

    const QString key = key_name_from_code(code);
    if (key.isEmpty())
    {
        emit note(word(words, QStringLiteral("shortcut_record_unknown_key"),
            QStringLiteral("This key cannot be used for a shortcut here. Press another one.")));
        return;
    }

    Shortcut shortcut;
    shortcut.key = key;
    // In the one order, so that it compares equal to the config's.
    for (const QString& known : all_modifiers())
    {
        if (held_modifiers.contains(known))
        {
            shortcut.modifiers << known;
        }
    }
    emit note(QString());
    stopRecording();
    emit recorded(shortcut);
}

bool ShortcutRecorder::event(QEvent* event)
{
    if (recording)
    {
        switch (event->type())
        {
        case QEvent::ShortcutOverride:
            // The window's own shortcuts (Escape for Cancel, Return for OK)
            // must not take the keys that are being recorded.
            event->accept();
            return true;
        case QEvent::KeyPress:
        case QEvent::KeyRelease: {
            // Handled here, not in keyPressEvent: the base class would
            // give Tab to the next widget before it got that far.
            auto* key_event = static_cast<QKeyEvent*>(event);
            handleKey(event->type() == QEvent::KeyPress, key_event->nativeScanCode(), key_event->isAutoRepeat());
            return true;
        }
        default:
            break;
        }
    }
    return QPushButton::event(event);
}

void ShortcutRecorder::focusInEvent(QFocusEvent* event)
{
    QPushButton::focusInEvent(event);
    // A click starts recording by itself.
    if (event->reason() != Qt::MouseFocusReason)
    {
        startRecording();
    }
}

void ShortcutRecorder::focusOutEvent(QFocusEvent* event)
{
    // Without the focus no key arrives here, and the compositor must have
    // its shortcuts back.
    stopRecording();
    QPushButton::focusOutEvent(event);
}
