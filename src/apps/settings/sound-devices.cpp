#include "sound-devices.hpp"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>

// How long the listing may take before the pane goes on without it.
static const int listing_patience_ms = 2000;

// What a node of each kind calls itself (its "media.class"), and the keys
// under which the session manager states the device in use and reads the
// user's choice.
struct KindNames
{
    const char* media_class;
    const char* in_use_key;
    const char* choice_key;
};
static KindNames names_of(SoundDevices::Kind kind)
{
    if (kind == SoundDevices::Kind::Output)
    {
        return {"Audio/Sink", "default.audio.sink", "default.configured.audio.sink"};
    }
    return {"Audio/Source", "default.audio.source", "default.configured.audio.source"};
}

QList<SoundDevices::Device> SoundDevices::all(Kind kind)
{
    QProcess listing;
    listing.start(QStringLiteral("pw-dump"), QStringList());
    if (!listing.waitForFinished(listing_patience_ms))
    {
        listing.kill();
        return {};
    }
    const QJsonArray objects = QJsonDocument::fromJson(listing.readAllStandardOutput()).array();
    const KindNames names = names_of(kind);

    QList<Device> devices;
    QString in_use;
    for (const QJsonValue& entry : objects)
    {
        const QJsonObject object = entry.toObject();
        const QString type = object.value(QLatin1String("type")).toString();

        if (type == QLatin1String("PipeWire:Interface:Node"))
        {
            const QJsonObject properties =
                object.value(QLatin1String("info")).toObject().value(QLatin1String("props")).toObject();
            // ("Audio/Source/Virtual" is a microphone made by a program,
            // a noise filter for one; it is a choice like the others.)
            const QString media_class = properties.value(QLatin1String("media.class")).toString();
            if (media_class != QLatin1String(names.media_class) &&
                media_class != QLatin1String(names.media_class) + QLatin1String("/Virtual"))
            {
                continue;
            }
            Device device;
            device.name = properties.value(QLatin1String("node.name")).toString();
            device.description = properties.value(QLatin1String("node.description")).toString(device.name);
            devices.append(device);
        }
        else if (type == QLatin1String("PipeWire:Interface:Metadata"))
        {
            // The store named "default" says which device is in use:
            // { "key": "default.audio.sink", "value": { "name": "..." } }.
            const QJsonArray entries = object.value(QLatin1String("metadata")).toArray();
            for (const QJsonValue& kept : entries)
            {
                if (kept.toObject().value(QLatin1String("key")).toString() == QLatin1String(names.in_use_key))
                {
                    in_use = kept.toObject()
                                 .value(QLatin1String("value"))
                                 .toObject()
                                 .value(QLatin1String("name"))
                                 .toString();
                }
            }
        }
    }

    for (Device& device : devices)
    {
        device.in_use = device.name == in_use;
    }
    return devices;
}

void SoundDevices::choose(Kind kind, const QString& name)
{
    QJsonObject value;
    value.insert(QLatin1String("name"), name);
    // "0" is the subject: the choice is about the whole, not one node.
    QProcess::startDetached(
        QStringLiteral("pw-metadata"), {
                                           QStringLiteral("-n"),
                                           QStringLiteral("default"),
                                           QStringLiteral("0"),
                                           QLatin1String(names_of(kind).choice_key),
                                           QString::fromUtf8(QJsonDocument(value).toJson(QJsonDocument::Compact)),
                                           QStringLiteral("Spa:String:JSON"),
                                       });
}
