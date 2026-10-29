#include "sound-pane.hpp"

#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QFileInfo>
#include <QFormLayout>
#include <QProcess>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSlider>
#include <QStandardPaths>

#include "choices.hpp"
#include "pane-parts.hpp"
#include "sound-devices.hpp"

QStringList SoundPane::packs()
{
    QStringList names;
    const QStringList data_folders = QStandardPaths::standardLocations(QStandardPaths::GenericDataLocation);
    for (const QString& data_folder : data_folders)
    {
        const QDir sounds(data_folder + QStringLiteral("/sounds"));
        const QStringList folders = sounds.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
        for (const QString& folder : folders)
        {
            if (sounds.exists(folder + QStringLiteral("/stereo")) && !names.contains(folder))
            {
                names << folder;
            }
        }
    }

    names.sort(Qt::CaseInsensitive);
    return names;
}

QStringList SoundPane::alertsOf(const QString& pack)
{
    QStringList names;
    const QStringList data_folders = QStandardPaths::standardLocations(QStandardPaths::GenericDataLocation);
    for (const QString& data_folder : data_folders)
    {
        const QDir sounds(data_folder + QStringLiteral("/sounds/") + pack + QStringLiteral("/stereo"));
        if (!sounds.exists())
        {
            continue;
        }
        // The first folder that has the pack is the pack (the shell's rule).
        const QFileInfoList files = sounds.entryInfoList({QStringLiteral("alert-*")}, QDir::Files, QDir::Name);
        for (const QFileInfo& file : files)
        {
            // "alert-Funk.wav" -> "Funk"
            names << file.completeBaseName().mid(QStringLiteral("alert-").size());
        }
        break;
    }

    return names;
}

SoundPane::SoundPane(Choices& choices, QWidget* parent) : QWidget(parent)
{
    const QVariantMap words = choices.configTable(QStringLiteral("settings")).value(QStringLiteral("words")).toMap();
    const auto word = [&words](const char* key, const char* fallback) {
        return words.value(QLatin1String(key), QString::fromUtf8(fallback)).toString();
    };

    auto* enabled = new QCheckBox(word("sound_enabled", "Play the desktop's sounds"));
    const QVariant chosen_enabled = choices.value(QStringLiteral("sounds"), QStringLiteral("enabled"));
    enabled->setChecked(!chosen_enabled.isValid() || chosen_enabled.toBool());

    // The pack in use: the user's or the config's, else the theme's.
    QString current = choices.value(QStringLiteral("sounds"), QStringLiteral("pack")).toString();
    if (current.isEmpty())
    {
        current = choices.themeTable(QStringLiteral("sounds")).value(QStringLiteral("pack")).toString();
    }
    auto* pack = new QComboBox;
    pack->addItems(packs());
    pack->setCurrentText(current);
    pack->setEnabled(enabled->isChecked());

    auto* try_it = new QPushButton(word("sound_try", "Play a sound"));
    try_it->setEnabled(enabled->isChecked());
    connect(try_it, &QPushButton::clicked, this,
        []() { QProcess::startDetached(QStringLiteral("mavernix-sound"), {QStringLiteral("message")}); });

    // The alert sound, among those the pack in use brings (files named
    // "alert-NAME"); a pack without any offers no choice.
    auto* alert = new QComboBox;
    // The first entry stands for the pack's own alert sound: no choice.
    const QString packs_own = word("sound_alert_default", "The pack's own");
    const auto fill_alerts = [alert, packs_own, &choices](const QString& pack_name) {
        const QSignalBlocker quiet(alert);
        alert->clear();
        alert->addItem(packs_own);
        alert->addItems(alertsOf(pack_name));
        const int chosen = alert->findText(choices.value(QStringLiteral("sounds"), QStringLiteral("alert")).toString());
        alert->setCurrentIndex(chosen > 0 ? chosen : 0);
        alert->setEnabled(alert->count() > 1);
    };
    fill_alerts(current);
    connect(alert, &QComboBox::currentIndexChanged, this, [alert, &choices](int index) {
        // An empty name is "no choice": the pack's own.
        choices.set(QStringLiteral("sounds"), QStringLiteral("alert"), index > 0 ? alert->itemText(index) : QString());
        QProcess::startDetached(QStringLiteral("mavernix-sound"), {QStringLiteral("bell")});
    });

    auto* feedback = new QCheckBox(word("sound_volume_feedback", "Play a sound when the volume is changed"));
    const QVariant chosen_feedback = choices.value(QStringLiteral("sounds"), QStringLiteral("volume_feedback"));
    feedback->setChecked(!chosen_feedback.isValid() || chosen_feedback.toBool());
    connect(feedback, &QCheckBox::toggled, this,
        [&choices](bool on) { choices.set(QStringLiteral("sounds"), QStringLiteral("volume_feedback"), on); });

    connect(enabled, &QCheckBox::toggled, this, [&choices, pack, try_it, feedback](bool on) {
        pack->setEnabled(on);
        try_it->setEnabled(on);
        feedback->setEnabled(on);
        choices.set(QStringLiteral("sounds"), QStringLiteral("enabled"), on);
    });
    connect(pack, &QComboBox::currentTextChanged, this, [&choices, fill_alerts](const QString& name) {
        choices.set(QStringLiteral("sounds"), QStringLiteral("pack"), name);
        fill_alerts(name);
    });

    // Where sound goes to and comes from. A kind with no device at all
    // (a computer without a microphone) has its row greyed.
    const auto device_chooser = [this](SoundDevices::Kind kind) {
        auto* chooser = new QComboBox;
        for (const SoundDevices::Device& device : SoundDevices::all(kind))
        {
            chooser->addItem(device.description, device.name);
            if (device.in_use)
            {
                chooser->setCurrentIndex(chooser->count() - 1);
            }
        }
        chooser->setEnabled(chooser->count() > 0);
        connect(chooser, &QComboBox::activated, this,
            [chooser, kind](int index) { SoundDevices::choose(kind, chooser->itemData(index).toString()); });
        return chooser;
    };

    // How loud the output is, and how loud the microphone records, through
    // WirePlumber's wpctl; greyed if it is not installed.
    const QString quiet_word = word("sound_volume_low", "Quiet");
    const QString loud_word = word("sound_volume_high", "Loud");
    const auto volume_slider = [this, quiet_word, loud_word](const QString& target) {
        auto* slider = new QSlider(Qt::Horizontal);
        slider->setRange(0, 100);
        slider->setMinimumWidth(220);
        QProcess reading;
        reading.start(QStringLiteral("wpctl"), {QStringLiteral("get-volume"), target});
        // "Volume: 0.45" or "Volume: 0.45 [MUTED]".
        const bool read = reading.waitForFinished(1000) && reading.exitCode() == 0;
        const QStringList said = QString::fromUtf8(reading.readAllStandardOutput()).split(QLatin1Char(' '));
        slider->setEnabled(read && said.size() > 1);
        if (said.size() > 1)
        {
            slider->setValue(qRound(said[1].trimmed().toDouble() * 100));
        }
        connect(slider, &QSlider::valueChanged, this, [target](int percent) {
            QProcess::startDetached(QStringLiteral("wpctl"),
                {QStringLiteral("set-volume"), target, QString::number(percent / 100.0, 'f', 2)});
        });
        return with_end_words(slider, quiet_word, loud_word);
    };

    auto* form = new QFormLayout(this);
    form->setLabelAlignment(Qt::AlignRight | Qt::AlignTop);
    form->setFormAlignment(Qt::AlignHCenter | Qt::AlignTop);
    form->addRow(word("sound_output", "Output:"), device_chooser(SoundDevices::Kind::Output));
    form->addRow(word("sound_output_volume", "Output volume:"), volume_slider(QStringLiteral("@DEFAULT_AUDIO_SINK@")));
    form->addRow(word("sound_input", "Input:"), device_chooser(SoundDevices::Kind::Input));
    form->addRow(word("sound_input_volume", "Input volume:"), volume_slider(QStringLiteral("@DEFAULT_AUDIO_SOURCE@")));
    form->addRow(word("sound_alert", "Alert sound:"), alert);
    form->addRow(word("sound_pack", "Sound pack:"), pack);
    form->addRow(QString(), try_it);
    form->addRow(QString(), enabled);
    form->addRow(QString(), feedback);
}
