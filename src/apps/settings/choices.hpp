// The desktop's settings as this program sees and changes them.
//
// The session's JSON file (DESKTOP_SHELL_SETTINGS, written by
// src/session/make_shell_config.py) holds the config with the user's changes,
// the theme, and where those changes are kept. A change is written to the
// user's file, which the shell watches.

#pragma once

#include <QJsonObject>
#include <QList>
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
    // As value(), but as the user's file holds it now: another program (the
    // App Store, the shell) may have changed it since this one started.
    QVariant current(const QString& table, const QString& key) const;
    // Returns what went wrong writing the user's file, else empty.
    QString set(const QString& table, const QString& key, const QVariant& value);

    // The user's own values for these back to the desktop's: each a
    // { table, key }, or a whole table with an empty key. Returns what went
    // wrong, else empty.
    struct Key
    {
        QString table;
        QString key;
    };
    QString remove(const QList<Key>& keys);
    // Every change the user made, kept first as backupFile(). Returns what
    // went wrong, else empty.
    QString removeAll();
    QString backupFile() const;

    // The table's names as they stand now, without those removed (set to null).
    QStringList names(const QString& table) const;
    // A [shortcuts] or [commands] key as the config has it, before the user's changes.
    QVariant defaultOf(const QString& table, const QString& key) const;

    QVariantMap themeTable(const QString& table) const;
    QVariantMap configTable(const QString& table) const;
    // The machine's keyboard, used until a layout is chosen:
    // { layout, variant, model, options }, each only if set.
    QVariantMap systemKeyboard() const;
    // By name: each { directory, manifest, looks, settings }, as the
    // session finds them now.
    QVariantMap extensions() const;
    // The file the session rewrites when the extensions change; watch it.
    QString extensionsFile() const;
    // Where the user's changes are kept, which the shell writes too.
    QString userChoicesFile() const;
    QString themeDirectory() const;

    // defaultDesktopFile() holds which desktop starts when none is named.
    QStringList desktops() const;
    QString runningDesktop() const;
    QString defaultDesktopFile() const;

  private:
    // The config with the user's file as it is now laid over the desktop's.
    void reload();
    QString write(const QJsonObject& held);

    QVariantMap settings;
    // What has been changed since the program started: { table: { key: value } }.
    QVariantMap changed;
};
