// The choice of the desktop's picture: the config's [desktop] picture, a
// full path or a path inside the theme; empty for the theme's background.

#pragma once

#include <QString>
#include <QWidget>

class Choices;
class QListWidget;
class QListWidgetItem;

class PictureChooser : public QWidget
{
    Q_OBJECT

  public:
    PictureChooser(Choices& choices, QWidget* parent = nullptr);

  protected:
    void showEvent(QShowEvent* event) override;

  private:
    void addPicture(const QString& value);
    void markChosen();
    void choose(const QString& value);
    // Read in the background, so the window stays responsive.
    void loadThumbnail(QListWidgetItem* item);
    QString fileOf(const QString& value) const;
    // Empty if the theme's background is no picture.
    QString themeBackground() const;

    Choices& choices;
    QListWidget* list;
    // Not before the chooser is first shown: most visits to the settings
    // never open its pane.
    bool thumbnails_wanted = false;
};
