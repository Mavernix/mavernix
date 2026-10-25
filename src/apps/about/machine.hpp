// What this machine is made of, as short lines a person can read: its
// processor, memory, graphics, model, the disk it started from, and the
// system that runs on it.
//
// Everything is read from files any user may read (/proc, /sys, udev's
// notes, /etc/os-release); nothing here needs root or starts a program.
// A fact that cannot be found out is an empty text, and the window then
// leaves its row out.

#pragma once

#include <QObject>
#include <QString>

class Machine : public QObject
{
    Q_OBJECT

  public:
    // One fact by its name: "processor", "memory", "graphics", "computer",
    // "startup_disk", "os_name", "os_version", "kernel", "host_name".
    // Empty for a name that is not known or a fact that was not found.
    Q_INVOKABLE QString fact(const QString& name) const;

    QString processor() const;
    QString memory() const;
    QString graphics() const;
    QString computer() const;
    QString startupDisk() const;
    QString osName() const;
    QString osVersion() const;
    QString kernel() const;
    QString hostName() const;
};
