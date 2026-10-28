// The Bluetooth pane: the radio, known and nearby devices, and connecting,
// pairing and forgetting one. It talks to BlueZ over the system bus; nothing
// is kept in the desktop's settings.

#pragma once

#include <QMap>
#include <QString>
#include <QVariantMap>
#include <QWidget>

class Choices;
class QCheckBox;
class QLabel;
class QListWidget;
class QPushButton;
class QTimer;

class BluetoothPane : public QWidget
{
    Q_OBJECT

  public:
    BluetoothPane(Choices& choices, QWidget* parent = nullptr);

  protected:
    // Devices nearby are looked for only while the pane is on screen.
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;

  private slots:
    // The list is read anew shortly.
    void somethingChanged();

  private:
    void readAll();
    void showChosen();
    void call(const QString& path, const char* interface, const char* method);
    void setSearching(bool searching);

    QVariantMap words;
    QString word(const char* key, const char* fallback) const;

    QCheckBox* powered = nullptr;
    QListWidget* devices = nullptr;
    QPushButton* connect_button = nullptr;
    QPushButton* forget_button = nullptr;
    QLabel* state_line = nullptr;
    QTimer* reading_soon = nullptr;

    // Empty if there is no radio.
    QString adapter_path;
    // By object path.
    QMap<QString, QVariantMap> known;
    // The device being paired from here, to trust and connect once paired.
    QString pairing_path;
};
