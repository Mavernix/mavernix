// Finds a freedesktop sound theme (share/sounds/NAME/stereo/EVENT.wav, .oga
// or .ogg) in the XDG data folders, the user's first. `sounds` maps event
// name to file path; `folder` is "" if the pack was not found.
//
// Usage in QML:  SoundPack { name: "SeaLion" }

#pragma once

#include <QObject>
#include <QVariantMap>

class SoundPack : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString name READ name WRITE setName NOTIFY nameChanged)
    Q_PROPERTY(QString folder READ folder NOTIFY soundsChanged)
    Q_PROPERTY(QVariantMap sounds READ sounds NOTIFY soundsChanged)

  public:
    explicit SoundPack(QObject* parent = nullptr);

    QString name() const;
    void setName(const QString& name);
    QString folder() const;
    QVariantMap sounds() const;

  signals:
    void nameChanged();
    void soundsChanged();

  private:
    void look();

    QString packName;
    QString packFolder;
    QVariantMap found;
};
