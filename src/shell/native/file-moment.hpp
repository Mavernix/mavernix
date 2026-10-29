// The moment by which a folder's "date" orders sort a file: when it was
// added, last modified or created. Shared by FolderContents and FileKind.

#pragma once

#include <QDateTime>
#include <QFileInfo>
#include <QString>

// `order` is "added", "modified" or "created"; anything else counts as modified.
QDateTime moment_for(const QFileInfo& file, const QString& order);
