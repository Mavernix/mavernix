// What each pane's Restore Defaults button takes back: the user's own values
// that the pane writes, as { table, key }, or a whole table with an empty
// key. A pane not listed (theme, network, bluetooth, extensions, defaults)
// has no such button: what it changes is not a choice of this kind.
//
// A pane that writes a new key must be added here too.

#pragma once

#include <QList>
#include <QString>

#include "choices.hpp"

QList<Choices::Key> pane_choices(const QString& pane);
