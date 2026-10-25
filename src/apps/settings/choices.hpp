// The desktop's settings as this program sees and changes them.
//
// The session's JSON file (DESKTOP_SHELL_SETTINGS, written by
// src/session/make_shell_config.py) holds the config with the user's changes,
// the theme, and where those changes are kept. A change is written to the
// user's file, which the shell watches.

#pragma once

#include <QString>
#include <QVariant>
#include <QVariantMap>

class Choices
{
  public:
    // Ends the program with a message if there is no settings file.
    Choices();

    // Invalid if the desktop does not set it.
    QVariant value(const QString& table, const QString& key) const;
    // Returns what went wrong writing the user's file, else empty.
    QString set(const QString& table, const QString& key, const QVariant& value);

    // The table's names as they stand now, without those removed (set to null).
    QStringList names(const QString& table) const;
    // A [shortcuts] or [commands] key as the config has it, before the user's changes.
    QVariant defaultOf(const QString& table, const QString& key) const;

    QVariantMap themeTable(const QString& table) const;
    QVariantMap configTable(const QString& table) const;
    // The machine's keyboard, used until a layout is chosen:
    // { layout, variant, model, options }, each only if set.
    QVariantMap systemKeyboard() const;
    // By name: each { directory, manifest, looks, settings }.
    QVariantMap extensions() const;
    QString themeDirectory() const;

    // defaultDesktopFile() holds which desktop starts when none is named.
    QStringList desktops() const;
    QString runningDesktop() const;
    QString defaultDesktopFile() const;

  private:
    QVariantMap settings;
    // What has been changed since the program started: { table: { key: value } }.
    QVariantMap changed;
};
