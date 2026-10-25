// Which loudspeakers and microphones there are, which is in use, and
// choosing another.
//
// Uses PipeWire's own tools, present wherever PipeWire is: `pw-dump` lists
// everything as JSON, `pw-metadata` writes the choice for the session manager.
// (The library would need some hundred lines of callbacks; the PulseAudio
// tools need pipewire-pulse running.)

#pragma once

#include <QList>
#include <QString>

namespace SoundDevices
{
enum class Kind
{
    Output,
    Input,
};

struct Device
{
    // The name PipeWire knows it by, and the one for people.
    QString name;
    QString description;
    // Whether sound goes to it (or comes from it) at present.
    bool in_use = false;
};

// The devices of a kind; empty if PipeWire does not answer.
QList<Device> all(Kind kind);

// Makes a device the one in use, by its PipeWire name.
void choose(Kind kind, const QString& name);
} // namespace SoundDevices
