// The settings of the dock: how large its icons are, whether they grow
// under the pointer and how large they get then.

#pragma once

#include <QWidget>

class Choices;
class QCheckBox;
class QFileSystemWatcher;
class QSlider;

class DockPane : public QWidget
{
    Q_OBJECT

  public:
    DockPane(Choices& choices, QWidget* parent = nullptr);

  private:
    // The user's or the config's, else the theme's for this screen.
    int currentIconSize() const;
    // The dock's settings as the user's file holds them now: the dock's own
    // menu and divider change them too, while this window is open.
    void showCurrent();

    Choices& choices;
    QSlider* size = nullptr;
    QCheckBox* magnification = nullptr;
    QSlider* magnified_size = nullptr;
    QCheckBox* hides = nullptr;
    QFileSystemWatcher* watcher = nullptr;
};
