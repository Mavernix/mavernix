// Keys as the compositor (Wayfire) writes them, and as a person reads them.
//
// The config holds a shortcut as text such as "<ctrl> <alt> KEY_LEFT", the key
// named as in the Linux kernel's list. Alternatives are separated by a bar
// ("KEY_F3 | <ctrl> KEY_UP"). Plain text work, tested in tests/test-keyboard.cpp.

#pragma once

#include <QString>
#include <QStringList>

struct Shortcut
{
    // Each one of "ctrl", "alt", "shift", "super".
    QStringList modifiers;
    // "KEY_LEFT"; empty for no shortcut.
    QString key;

    bool isEmpty() const { return key.isEmpty(); }
};

// The modifiers a shortcut may hold, in the order they are written and shown.
QStringList all_modifiers();

// The first alternative in `text`; empty if there is none that makes sense.
Shortcut parse_shortcut(const QString& text);
QList<Shortcut> parse_alternatives(const QString& text);

// The config's text for a shortcut, modifiers in a fixed order so that equal
// shortcuts compare as equal text.
QString write_shortcut(const Shortcut& shortcut);

bool has_alternative(const QString& text, const Shortcut& shortcut);

// The keys a person can pick from ("KEY_A"), in the order they are offered.
QStringList choosable_keys();

// "KEY_PAGEUP" is "Page Up"; the caller looks for the theme's wording first.
QString plain_key_label(const QString& key);

// Whether the key alone types a character or moves the text cursor; such a
// key needs a modifier besides Shift, or every program loses it.
bool is_typing_key(const QString& key);

// The kernel's number for a key (linux/input-event-codes.h) and back. The
// compositor binds physical keys, not what the layout prints on them.
// Empty / -1 for a key that is not choosable.
QString key_name_from_code(int evdev_code);
int key_code_from_name(const QString& key);

// "ctrl", "alt", "shift" or "super" for a modifier key (left or right), else empty.
QString modifier_from_code(int evdev_code);

// Whether nearly every program uses this shortcut itself (copy, paste, save).
bool is_common_program_key(const Shortcut& shortcut);
