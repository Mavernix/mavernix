// The programs installed on this computer, and which of them opens which
// kind of file, as GLib's GIO knows it (desktop entries, mimeapps.list).

#pragma once

#include <QIcon>
#include <QList>
#include <QString>
#include <QStringList>

struct InstalledProgram
{
    // The desktop entry's name ("org.kde.gwenview.desktop").
    QString id;
    QString name;
    // Without the entry's file name fields ("%U").
    QString command;
    QIcon icon;
};

// Every program meant to be seen in menus, by name.
QList<InstalledProgram> installed_programs();

// The programs that can open this kind of file ("image/png"), by name.
QList<InstalledProgram> programs_for_kind(const QString& kind);

// Empty if none is set or installed.
QString usual_program_id(const QString& kind);

// Returns what went wrong, else empty.
QString make_usual_program(const QString& id, const QStringList& kinds);
