#include "keyboard-layouts.hpp"

#include <QFile>
#include <QXmlStreamReader>

#include <algorithm>

QStringList KeyboardLayouts::systemFiles()
{
    // The compositor composes its keyboard from the rules "evdev"
    // (Wayfire's default); the second file holds layouts that came later.
    return {
        QStringLiteral("/usr/share/X11/xkb/rules/evdev.xml"),
        QStringLiteral("/usr/share/X11/xkb/rules/evdev.extras.xml"),
    };
}

// The text of the element the reader is at, up to its end.
static QString text_of(QXmlStreamReader& reader)
{
    return reader.readElementText().trimmed();
}

// Reads one <configItem>: the name and the description of a layout or variant.
static void read_config_item(QXmlStreamReader& reader, QString& name, QString& description)
{
    while (reader.readNextStartElement())
    {
        if (reader.name() == QLatin1String("name"))
        {
            name = text_of(reader);
        }
        else if (reader.name() == QLatin1String("description"))
        {
            description = text_of(reader);
        }
        else
        {
            reader.skipCurrentElement();
        }
    }
}

KeyboardLayouts KeyboardLayouts::read(const QStringList& files, QString* error)
{
    KeyboardLayouts result;
    QStringList unreadable;

    for (const QString& path : files)
    {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly))
        {
            unreadable << path;
            continue;
        }

        // The files' shape: <xkbConfigRegistry><layoutList><layout>
        //   <configItem><name/><description/></configItem>
        //   <variantList><variant><configItem>...</configItem></variant>...
        QXmlStreamReader reader(&file);
        while (!reader.atEnd() && !reader.hasError())
        {
            reader.readNext();
            if (!reader.isStartElement() || reader.name() != QLatin1String("layout"))
            {
                continue;
            }

            Layout layout;
            while (reader.readNextStartElement())
            {
                if (reader.name() == QLatin1String("configItem"))
                {
                    read_config_item(reader, layout.name, layout.description);
                }
                else if (reader.name() == QLatin1String("variantList"))
                {
                    while (reader.readNextStartElement())
                    {
                        Variant variant;
                        // <variant><configItem>
                        while (reader.readNextStartElement())
                        {
                            read_config_item(reader, variant.name, variant.description);
                        }
                        layout.variants.append(variant);
                    }
                }
                else
                {
                    reader.skipCurrentElement();
                }
            }
            if (!layout.name.isEmpty())
            {
                result.layouts.append(layout);
            }
        }
        if (reader.hasError())
        {
            unreadable << QStringLiteral("%1 (%2)").arg(path, reader.errorString());
        }
    }

    if (result.layouts.isEmpty() && error)
    {
        *error = QStringLiteral("No keyboard layouts found; could not read %1. "
                                "Check that xkeyboard-config is installed.")
                     .arg(unreadable.join(QStringLiteral(", ")));
    }
    return result;
}

QList<InputSource> KeyboardLayouts::all() const
{
    QList<InputSource> sources;
    for (const Layout& layout : layouts)
    {
        sources.append({layout.name, QString()});
        for (const Variant& variant : layout.variants)
        {
            sources.append({layout.name, variant.name});
        }
    }

    std::sort(sources.begin(), sources.end(),
        [this](const InputSource& a, const InputSource& b) { return describe(a).localeAwareCompare(describe(b)) < 0; });
    return sources;
}

QString KeyboardLayouts::describe(const InputSource& source) const
{
    for (const Layout& layout : layouts)
    {
        if (layout.name != source.layout)
        {
            continue;
        }
        if (source.variant.isEmpty())
        {
            return layout.description;
        }
        for (const Variant& variant : layout.variants)
        {
            if (variant.name == source.variant)
            {
                return variant.description;
            }
        }
    }

    return source.variant.isEmpty() ? source.layout : QStringLiteral("%1 (%2)").arg(source.layout, source.variant);
}

QList<InputSource> parse_sources(const QString& layouts, const QString& variants)
{
    const QStringList layout_names = layouts.split(QLatin1Char(','));
    const QStringList variant_names = variants.split(QLatin1Char(','));

    QList<InputSource> sources;
    for (int index = 0; index < layout_names.size(); index++)
    {
        const QString layout = layout_names[index].trimmed();
        if (layout.isEmpty())
        {
            continue;
        }
        sources.append({layout, variant_names.value(index).trimmed()});
    }
    return sources;
}

QString layouts_text(const QList<InputSource>& sources)
{
    QStringList names;
    for (const InputSource& source : sources)
    {
        names << source.layout;
    }
    return names.join(QLatin1Char(','));
}

QString variants_text(const QList<InputSource>& sources)
{
    QStringList names;
    bool any = false;
    for (const InputSource& source : sources)
    {
        names << source.variant;
        any = any || !source.variant.isEmpty();
    }
    // "," for two plain layouts would still be a variant list; none at all
    // is better said by nothing.
    return any ? names.join(QLatin1Char(',')) : QString();
}
