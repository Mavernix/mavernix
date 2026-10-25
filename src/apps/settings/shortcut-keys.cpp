#include "shortcut-keys.hpp"

#include <QRegularExpression>

#include <linux/input-event-codes.h>

QStringList all_modifiers()
{
    // The order the config writes them in ("<ctrl> <alt> ...", "<super> <shift> ...").
    return {QStringLiteral("ctrl"), QStringLiteral("alt"), QStringLiteral("super"), QStringLiteral("shift")};
}

// Names are the kernel's (linux/input-event-codes.h), as the compositor reads them.
QStringList choosable_keys()
{
    QStringList keys;
    for (char letter = 'A'; letter <= 'Z'; letter++)
    {
        keys << QStringLiteral("KEY_") + QLatin1Char(letter);
    }
    for (int digit = 0; digit <= 9; digit++)
    {
        keys << QStringLiteral("KEY_%1").arg(digit);
    }
    for (int number = 1; number <= 12; number++)
    {
        keys << QStringLiteral("KEY_F%1").arg(number);
    }
    keys << QStringLiteral("KEY_UP") << QStringLiteral("KEY_DOWN") << QStringLiteral("KEY_LEFT")
         << QStringLiteral("KEY_RIGHT") << QStringLiteral("KEY_SPACE") << QStringLiteral("KEY_TAB")
         << QStringLiteral("KEY_ENTER") << QStringLiteral("KEY_ESC") << QStringLiteral("KEY_BACKSPACE")
         << QStringLiteral("KEY_DELETE") << QStringLiteral("KEY_INSERT") << QStringLiteral("KEY_HOME")
         << QStringLiteral("KEY_END") << QStringLiteral("KEY_PAGEUP") << QStringLiteral("KEY_PAGEDOWN");
    keys << QStringLiteral("KEY_GRAVE") << QStringLiteral("KEY_MINUS") << QStringLiteral("KEY_EQUAL")
         << QStringLiteral("KEY_LEFTBRACE") << QStringLiteral("KEY_RIGHTBRACE") << QStringLiteral("KEY_BACKSLASH")
         << QStringLiteral("KEY_SEMICOLON") << QStringLiteral("KEY_APOSTROPHE") << QStringLiteral("KEY_COMMA")
         << QStringLiteral("KEY_DOT") << QStringLiteral("KEY_SLASH");
    // A keyboard's "Print" key sends KEY_SYSRQ.
    keys << QStringLiteral("KEY_SYSRQ") << QStringLiteral("KEY_VOLUMEUP") << QStringLiteral("KEY_VOLUMEDOWN")
         << QStringLiteral("KEY_MUTE") << QStringLiteral("KEY_BRIGHTNESSUP") << QStringLiteral("KEY_BRIGHTNESSDOWN")
         << QStringLiteral("KEY_PLAYPAUSE") << QStringLiteral("KEY_NEXTSONG") << QStringLiteral("KEY_PREVIOUSSONG");
    return keys;
}

// The header's own constants, so the table cannot drift from them;
// tests/test-keyboard.cpp checks that no offered key is missing here.
static const struct
{
    const char* name;
    int code;
} key_codes[] = {
    {"KEY_A", KEY_A},
    {"KEY_B", KEY_B},
    {"KEY_C", KEY_C},
    {"KEY_D", KEY_D},
    {"KEY_E", KEY_E},
    {"KEY_F", KEY_F},
    {"KEY_G", KEY_G},
    {"KEY_H", KEY_H},
    {"KEY_I", KEY_I},
    {"KEY_J", KEY_J},
    {"KEY_K", KEY_K},
    {"KEY_L", KEY_L},
    {"KEY_M", KEY_M},
    {"KEY_N", KEY_N},
    {"KEY_O", KEY_O},
    {"KEY_P", KEY_P},
    {"KEY_Q", KEY_Q},
    {"KEY_R", KEY_R},
    {"KEY_S", KEY_S},
    {"KEY_T", KEY_T},
    {"KEY_U", KEY_U},
    {"KEY_V", KEY_V},
    {"KEY_W", KEY_W},
    {"KEY_X", KEY_X},
    {"KEY_Y", KEY_Y},
    {"KEY_Z", KEY_Z},
    {"KEY_0", KEY_0},
    {"KEY_1", KEY_1},
    {"KEY_2", KEY_2},
    {"KEY_3", KEY_3},
    {"KEY_4", KEY_4},
    {"KEY_5", KEY_5},
    {"KEY_6", KEY_6},
    {"KEY_7", KEY_7},
    {"KEY_8", KEY_8},
    {"KEY_9", KEY_9},
    {"KEY_F1", KEY_F1},
    {"KEY_F2", KEY_F2},
    {"KEY_F3", KEY_F3},
    {"KEY_F4", KEY_F4},
    {"KEY_F5", KEY_F5},
    {"KEY_F6", KEY_F6},
    {"KEY_F7", KEY_F7},
    {"KEY_F8", KEY_F8},
    {"KEY_F9", KEY_F9},
    {"KEY_F10", KEY_F10},
    {"KEY_F11", KEY_F11},
    {"KEY_F12", KEY_F12},
    {"KEY_UP", KEY_UP},
    {"KEY_DOWN", KEY_DOWN},
    {"KEY_LEFT", KEY_LEFT},
    {"KEY_RIGHT", KEY_RIGHT},
    {"KEY_SPACE", KEY_SPACE},
    {"KEY_TAB", KEY_TAB},
    {"KEY_ENTER", KEY_ENTER},
    {"KEY_ESC", KEY_ESC},
    {"KEY_BACKSPACE", KEY_BACKSPACE},
    {"KEY_DELETE", KEY_DELETE},
    {"KEY_INSERT", KEY_INSERT},
    {"KEY_HOME", KEY_HOME},
    {"KEY_END", KEY_END},
    {"KEY_PAGEUP", KEY_PAGEUP},
    {"KEY_PAGEDOWN", KEY_PAGEDOWN},
    {"KEY_GRAVE", KEY_GRAVE},
    {"KEY_MINUS", KEY_MINUS},
    {"KEY_EQUAL", KEY_EQUAL},
    {"KEY_LEFTBRACE", KEY_LEFTBRACE},
    {"KEY_RIGHTBRACE", KEY_RIGHTBRACE},
    {"KEY_BACKSLASH", KEY_BACKSLASH},
    {"KEY_SEMICOLON", KEY_SEMICOLON},
    {"KEY_APOSTROPHE", KEY_APOSTROPHE},
    {"KEY_COMMA", KEY_COMMA},
    {"KEY_DOT", KEY_DOT},
    {"KEY_SLASH", KEY_SLASH},
    {"KEY_SYSRQ", KEY_SYSRQ},
    {"KEY_VOLUMEUP", KEY_VOLUMEUP},
    {"KEY_VOLUMEDOWN", KEY_VOLUMEDOWN},
    {"KEY_MUTE", KEY_MUTE},
    {"KEY_BRIGHTNESSUP", KEY_BRIGHTNESSUP},
    {"KEY_BRIGHTNESSDOWN", KEY_BRIGHTNESSDOWN},
    {"KEY_PLAYPAUSE", KEY_PLAYPAUSE},
    {"KEY_NEXTSONG", KEY_NEXTSONG},
    {"KEY_PREVIOUSSONG", KEY_PREVIOUSSONG},
};

QString key_name_from_code(int evdev_code)
{
    for (const auto& entry : key_codes)
    {
        if (entry.code == evdev_code)
        {
            return QString::fromLatin1(entry.name);
        }
    }
    return QString();
}

int key_code_from_name(const QString& key)
{
    for (const auto& entry : key_codes)
    {
        if (key == QLatin1String(entry.name))
        {
            return entry.code;
        }
    }
    return -1;
}

QString modifier_from_code(int evdev_code)
{
    switch (evdev_code)
    {
    case KEY_LEFTCTRL:
    case KEY_RIGHTCTRL:
        return QStringLiteral("ctrl");
    case KEY_LEFTALT:
    case KEY_RIGHTALT:
        return QStringLiteral("alt");
    case KEY_LEFTSHIFT:
    case KEY_RIGHTSHIFT:
        return QStringLiteral("shift");
    case KEY_LEFTMETA:
    case KEY_RIGHTMETA:
        return QStringLiteral("super");
    default:
        return QString();
    }
}

bool is_common_program_key(const Shortcut& shortcut)
{
    // Ctrl (or Ctrl+Shift) with the letters of nearly every program's edit,
    // file and tab commands.
    static const QStringList ctrl_letters = {
        QStringLiteral("KEY_A"),
        QStringLiteral("KEY_C"),
        QStringLiteral("KEY_V"),
        QStringLiteral("KEY_X"),
        QStringLiteral("KEY_Z"),
        QStringLiteral("KEY_Y"),
        QStringLiteral("KEY_S"),
        QStringLiteral("KEY_F"),
        QStringLiteral("KEY_N"),
        QStringLiteral("KEY_O"),
        QStringLiteral("KEY_P"),
        QStringLiteral("KEY_W"),
        QStringLiteral("KEY_T"),
        QStringLiteral("KEY_Q"),
    };
    const bool only_ctrl_and_shift = shortcut.modifiers.contains(QStringLiteral("ctrl")) &&
                                     !shortcut.modifiers.contains(QStringLiteral("alt")) &&
                                     !shortcut.modifiers.contains(QStringLiteral("super"));
    if (only_ctrl_and_shift && ctrl_letters.contains(shortcut.key))
    {
        return true;
    }

    // Alt+F4 and Alt+Tab are habits on every desktop.
    const bool only_alt = shortcut.modifiers == QStringList{QStringLiteral("alt")};
    return only_alt && (shortcut.key == QLatin1String("KEY_F4") || shortcut.key == QLatin1String("KEY_TAB"));
}

// Labels for keys whose name says little ("KEY_PAGEUP").
static QString special_label(const QString& name)
{
    static const struct
    {
        const char* name;
        const char* label;
    } labels[] = {
        {"KEY_UP", "Up"},
        {"KEY_DOWN", "Down"},
        {"KEY_LEFT", "Left"},
        {"KEY_RIGHT", "Right"},
        {"KEY_SPACE", "Space"},
        {"KEY_TAB", "Tab"},
        {"KEY_ENTER", "Return"},
        {"KEY_ESC", "Escape"},
        {"KEY_BACKSPACE", "Backspace"},
        {"KEY_DELETE", "Delete"},
        {"KEY_INSERT", "Insert"},
        {"KEY_HOME", "Home"},
        {"KEY_END", "End"},
        {"KEY_PAGEUP", "Page Up"},
        {"KEY_PAGEDOWN", "Page Down"},
        {"KEY_GRAVE", "`"},
        {"KEY_MINUS", "-"},
        {"KEY_EQUAL", "="},
        {"KEY_LEFTBRACE", "["},
        {"KEY_RIGHTBRACE", "]"},
        {"KEY_BACKSLASH", "\\"},
        {"KEY_SEMICOLON", ";"},
        {"KEY_APOSTROPHE", "'"},
        {"KEY_COMMA", ","},
        {"KEY_DOT", "."},
        {"KEY_SLASH", "/"},
        {"KEY_SYSRQ", "Print Screen"},
        {"KEY_VOLUMEUP", "Volume Up"},
        {"KEY_VOLUMEDOWN", "Volume Down"},
        {"KEY_MUTE", "Mute"},
        {"KEY_BRIGHTNESSUP", "Brightness Up"},
        {"KEY_BRIGHTNESSDOWN", "Brightness Down"},
        {"KEY_PLAYPAUSE", "Play/Pause"},
        {"KEY_NEXTSONG", "Next Track"},
        {"KEY_PREVIOUSSONG", "Previous Track"},
    };
    for (const auto& entry : labels)
    {
        if (name == QLatin1String(entry.name))
        {
            return QString::fromLatin1(entry.label);
        }
    }
    return QString();
}

QString plain_key_label(const QString& key)
{
    const QString special = special_label(key);
    if (!special.isEmpty())
    {
        return special;
    }
    // "KEY_KP1" is "KP1".
    return key.startsWith(QLatin1String("KEY_")) ? key.mid(4) : key;
}

bool is_typing_key(const QString& key)
{
    // Function keys, and the keys of a laptop that do something by
    // themselves, may stand alone.
    static const QRegularExpression function_key(QStringLiteral("^KEY_F[0-9]{1,2}$"));
    if (function_key.match(key).hasMatch())
    {
        return false;
    }
    static const QStringList stand_alone = {
        QStringLiteral("KEY_SYSRQ"),
        QStringLiteral("KEY_VOLUMEUP"),
        QStringLiteral("KEY_VOLUMEDOWN"),
        QStringLiteral("KEY_MUTE"),
        QStringLiteral("KEY_BRIGHTNESSUP"),
        QStringLiteral("KEY_BRIGHTNESSDOWN"),
        QStringLiteral("KEY_PLAYPAUSE"),
        QStringLiteral("KEY_NEXTSONG"),
        QStringLiteral("KEY_PREVIOUSSONG"),
    };
    return !stand_alone.contains(key);
}

Shortcut parse_shortcut(const QString& text)
{
    Shortcut shortcut;
    // Only the first of several alternatives.
    const QStringList words = text.section(QLatin1Char('|'), 0, 0)
                                  .trimmed()
                                  .split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
    for (const QString& word : words)
    {
        if (word.startsWith(QLatin1Char('<')) && word.endsWith(QLatin1Char('>')))
        {
            const QString modifier = word.mid(1, word.size() - 2).toLower();
            if (!all_modifiers().contains(modifier))
            {
                return Shortcut();
            }
            if (!shortcut.modifiers.contains(modifier))
            {
                shortcut.modifiers << modifier;
            }
        }
        else if (word.startsWith(QLatin1String("KEY_")) && shortcut.key.isEmpty())
        {
            shortcut.key = word;
        }
        else
        {
            // A second key, or no key at all.
            return Shortcut();
        }
    }

    // Written in the one order, so that two spellings of a shortcut compare as the same.
    QStringList ordered;
    for (const QString& modifier : all_modifiers())
    {
        if (shortcut.modifiers.contains(modifier))
        {
            ordered << modifier;
        }
    }
    shortcut.modifiers = ordered;
    return shortcut;
}

QList<Shortcut> parse_alternatives(const QString& text)
{
    QList<Shortcut> alternatives;
    for (const QString& part : text.split(QLatin1Char('|'), Qt::SkipEmptyParts))
    {
        const Shortcut shortcut = parse_shortcut(part);
        if (!shortcut.isEmpty())
        {
            alternatives.append(shortcut);
        }
    }
    return alternatives;
}

QString write_shortcut(const Shortcut& shortcut)
{
    if (shortcut.isEmpty())
    {
        return QString();
    }

    QStringList words;
    for (const QString& modifier : all_modifiers())
    {
        if (shortcut.modifiers.contains(modifier))
        {
            words << QLatin1Char('<') + modifier + QLatin1Char('>');
        }
    }
    words << shortcut.key;
    return words.join(QLatin1Char(' '));
}

bool has_alternative(const QString& text, const Shortcut& shortcut)
{
    const QString wanted = write_shortcut(shortcut);
    for (const Shortcut& alternative : parse_alternatives(text))
    {
        if (write_shortcut(alternative) == wanted)
        {
            return true;
        }
    }
    return false;
}
