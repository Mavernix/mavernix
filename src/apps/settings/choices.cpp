#include "choices.hpp"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>

#include <cstdio>
#include <cstdlib>

// `defaults` with `changes` laid over it: a table in both is joined key by
// key, anything else in `changes` takes the place of what `defaults` had
// (as src/session/user_choices.py does it).
static QVariantMap laid_over(QVariantMap defaults, const QVariantMap& changes)
{
    for (auto change = changes.constBegin(); change != changes.constEnd(); ++change)
    {
        const QVariant held = defaults.value(change.key());
        const bool both_tables =
            (change.value().typeId() == QMetaType::QVariantMap) && (held.typeId() == QMetaType::QVariantMap);
        defaults.insert(
            change.key(), both_tables ? QVariant(laid_over(held.toMap(), change.value().toMap())) : change.value());
    }

    return defaults;
}

Choices::Choices()
{
    const QString path = qEnvironmentVariable("DESKTOP_SHELL_SETTINGS");
    QFile file(path);
    if (path.isEmpty() || !file.open(QIODevice::ReadOnly))
    {
        std::fprintf(stderr,
            "mavernix-settings: cannot read the session's settings "
            "(DESKTOP_SHELL_SETTINGS is \"%s\"). The program is meant to be started "
            "inside the desktop session, which sets it (start-desktop).\n",
            qPrintable(path));
        std::exit(1);
    }

    settings = QJsonDocument::fromJson(file.readAll()).object().toVariantMap();
    reload();
}

void Choices::reload()
{
    // The session's file is as old as the session; the user's changes as
    // they are now are laid over the desktop's own config, as at a session's
    // start. A session from before default_config was written has only the
    // config with the changes of its start.
    const QVariant defaults = settings.value(QStringLiteral("default_config"));
    QVariantMap config = defaults.isValid() ? defaults.toMap() : settings.value(QStringLiteral("config")).toMap();
    QFile choices_file(userChoicesFile());
    if (choices_file.open(QIODevice::ReadOnly))
    {
        config = laid_over(config, QJsonDocument::fromJson(choices_file.readAll()).object().toVariantMap());
    }
    settings.insert(QStringLiteral("config"), config);
    changed.clear();
}

QString Choices::write(const QJsonObject& held)
{
    // So that a reader never finds half a file.
    const QString path = userChoicesFile();
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(QJsonDocument(held).toJson()) < 0 || !file.commit())
    {
        return QStringLiteral("Could not write %1: %2").arg(path, file.errorString());
    }
    return QString();
}

QString Choices::remove(const QList<Key>& keys)
{
    QJsonObject held;
    QFile existing(userChoicesFile());
    if (existing.open(QIODevice::ReadOnly))
    {
        held = QJsonDocument::fromJson(existing.readAll()).object();
    }
    for (const Key& one : keys)
    {
        if (one.key.isEmpty())
        {
            held.remove(one.table);
            continue;
        }
        QJsonObject table = held.value(one.table).toObject();
        table.remove(one.key);
        if (table.isEmpty())
        {
            held.remove(one.table);
        }
        else
        {
            held.insert(one.table, table);
        }
    }
    const QString problem = write(held);
    reload();
    return problem;
}

QString Choices::backupFile() const
{
    return userChoicesFile() + QStringLiteral(".backup");
}

QString Choices::removeAll()
{
    const QString path = userChoicesFile();
    if (QFile::exists(path))
    {
        QFile::remove(backupFile());
        if (!QFile::copy(path, backupFile()))
        {
            return QStringLiteral("Could not keep a copy of %1 as %2").arg(path, backupFile());
        }
    }
    const QString problem = write(QJsonObject());
    reload();
    return problem;
}

QVariant Choices::value(const QString& table, const QString& key) const
{
    const QVariantMap changed_table = changed.value(table).toMap();
    if (changed_table.contains(key))
    {
        return changed_table.value(key);
    }

    return configTable(table).value(key);
}

QVariant Choices::current(const QString& table, const QString& key) const
{
    QFile file(userChoicesFile());
    if (file.open(QIODevice::ReadOnly))
    {
        const QJsonObject held_table = QJsonDocument::fromJson(file.readAll()).object().value(table).toObject();
        if (held_table.contains(key))
        {
            return held_table.value(key).toVariant();
        }
    }

    return value(table, key);
}

QString Choices::set(const QString& table, const QString& key, const QVariant& value)
{
    QVariantMap changed_table = changed.value(table).toMap();
    changed_table.insert(key, value);
    changed.insert(table, changed_table);

    // The shell writes this file too (the dock's programs), so it is read
    // afresh and only one key is changed.
    const QString path = settings.value(QStringLiteral("user_choices_file")).toString();
    QJsonObject held;
    QFile existing(path);
    if (existing.open(QIODevice::ReadOnly))
    {
        held = QJsonDocument::fromJson(existing.readAll()).object();
    }

    QJsonObject held_table = held.value(table).toObject();
    held_table.insert(key, QJsonValue::fromVariant(value));
    held.insert(table, held_table);
    return write(held);
}

QStringList Choices::names(const QString& table) const
{
    QVariantMap entries = configTable(table);
    const QVariantMap changed_table = changed.value(table).toMap();
    for (auto change = changed_table.constBegin(); change != changed_table.constEnd(); ++change)
    {
        entries.insert(change.key(), change.value());
    }

    QStringList names;
    for (auto entry = entries.constBegin(); entry != entries.constEnd(); ++entry)
    {
        if (entry.value().isValid() && !entry.value().isNull())
        {
            names << entry.key();
        }
    }
    return names;
}

QVariant Choices::defaultOf(const QString& table, const QString& key) const
{
    return settings.value(QStringLiteral("shortcut_defaults")).toMap().value(table).toMap().value(key);
}

QVariantMap Choices::themeTable(const QString& table) const
{
    return settings.value(QStringLiteral("theme")).toMap().value(table).toMap();
}

QVariantMap Choices::configTable(const QString& table) const
{
    return settings.value(QStringLiteral("config")).toMap().value(table).toMap();
}

QVariantMap Choices::systemKeyboard() const
{
    return settings.value(QStringLiteral("system_keyboard")).toMap();
}

QVariantMap Choices::extensions() const
{
    // What the session finds now, extensions the App Store installed since
    // it started included; else the list as it started.
    QFile file(extensionsFile());
    if (file.open(QIODevice::ReadOnly))
    {
        const QJsonObject held = QJsonDocument::fromJson(file.readAll()).object();
        if (held.value(QStringLiteral("extensions")).isObject())
        {
            return held.value(QStringLiteral("extensions")).toObject().toVariantMap();
        }
    }

    return settings.value(QStringLiteral("extensions")).toMap();
}

QString Choices::extensionsFile() const
{
    return settings.value(QStringLiteral("extensions_file")).toString();
}

QString Choices::userChoicesFile() const
{
    return settings.value(QStringLiteral("user_choices_file")).toString();
}

QString Choices::themeDirectory() const
{
    return settings.value(QStringLiteral("theme_directory")).toString();
}

QStringList Choices::desktops() const
{
    return settings.value(QStringLiteral("desktops")).toStringList();
}

QString Choices::runningDesktop() const
{
    return settings.value(QStringLiteral("desktop")).toString();
}

QString Choices::defaultDesktopFile() const
{
    return settings.value(QStringLiteral("default_desktop_file")).toString();
}
