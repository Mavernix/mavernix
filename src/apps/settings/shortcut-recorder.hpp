// A button that records a shortcut from the keys the person presses.
//
// While it records, the compositor switches the desktop's own shortcuts off
// (compositor-link.hpp), so Super+Q arrives here instead of closing the window.
// The first non-modifier key ends it; Escape alone cancels. Keys are physical
// numbers, as the compositor binds them, so the layout does not matter.

#pragma once

#include <QPushButton>
#include <QSet>
#include <QStringList>
#include <QVariantMap>

#include "compositor-link.hpp"
#include "shortcut-keys.hpp"

class QKeyEvent;

class ShortcutRecorder : public QPushButton
{
    Q_OBJECT

  public:
    // Qt's native scan codes are X11/xkbcommon keycodes: the kernel's number plus 8.
    static const int scan_code_offset = 8;

    explicit ShortcutRecorder(const QVariantMap& words, QWidget* parent = nullptr);

    bool isRecording() const { return recording; }
    void startRecording();
    // Stops without a result.
    void stopRecording();

    // Public so that it can be tried without a keyboard.
    void handleKey(bool pressed, quint32 scan_code, bool auto_repeat);

  signals:
    void recorded(const Shortcut& shortcut);
    // Text for under the button; empty to clear it.
    void note(const QString& text);

  protected:
    bool event(QEvent* event) override;
    void focusInEvent(QFocusEvent* event) override;
    void focusOutEvent(QFocusEvent* event) override;

  private:
    void showIdle();
    void showHeld();
    void onLinkProblem(CompositorLink::Problem kind, const QString& detail);

    QVariantMap words;
    CompositorLink link;
    bool recording = false;
    // Kept from the key events themselves: Qt's modifier state is not
    // reliable on the events of the modifier keys.
    QSet<QString> held_modifiers;
    // Physical keys, as both Ctrl keys may be down.
    QSet<int> held_modifier_codes;
};
