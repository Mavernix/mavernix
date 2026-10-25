// The choice of the desktop's picture: the config's [desktop] picture, a
// full path or a path inside the theme; empty for the theme's background.

#pragma once

#include <QString>
#include <QWidget>

class Choices;
class QListWidget;

class PictureChooser : public QWidget
{
    Q_OBJECT

  public:
    PictureChooser(Choices& choices, QWidget* parent = nullptr);

  private:
    void addPicture(const QString& value);
    void markChosen();
    void choose(const QString& value);
    // One picture per call, so the window stays responsive.
    void loadNextThumbnail();
    QString fileOf(const QString& value) const;
    // Empty if the theme's background is no picture.
    QString themeBackground() const;

    Choices& choices;
    QListWidget* list;
    int next_thumbnail_row = 0;
};
