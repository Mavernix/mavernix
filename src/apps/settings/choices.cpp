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

    // The session's file is as old as the session; the user's later changes
    // are laid over it here, as at a session's start.
    QFile choices_file(settings.value(QStringLiteral("user_choices_file")).toString());
    if (choices_file.open(QIODevice::ReadOnly))
    {
        const QVariantMap held = QJsonDocument::fromJson(choices_file.readAll()).object().toVariantMap();
        QVariantMap config = settings.value(QStringLiteral("config")).toMap();
        config = laid_over(config, held);
        settings.insert(QStringLiteral("config"), config);
    }
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

    // So that a reader never finds half a file.
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(QJsonDocument(held).toJson()) < 0 || !file.commit())
    {
        return QStringLiteral("Could not write %1: %2").arg(path, file.errorString());
    }

    return QString();
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
    return settings.value(QStringLiteral("extensions")).toMap();
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
