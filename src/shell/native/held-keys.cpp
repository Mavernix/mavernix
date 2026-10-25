#include "held-keys.hpp"

#include <QGuiApplication>

bool HeldKeys::held(const QString& modifier) const
{
    const Qt::KeyboardModifiers now = QGuiApplication::queryKeyboardModifiers();
    if (modifier == QLatin1String("super"))
        return now.testFlag(Qt::MetaModifier);
    if (modifier == QLatin1String("alt"))
        return now.testFlag(Qt::AltModifier);
    if (modifier == QLatin1String("ctrl"))
        return now.testFlag(Qt::ControlModifier);
    if (modifier == QLatin1String("shift"))
        return now.testFlag(Qt::ShiftModifier);
    return false;
}
