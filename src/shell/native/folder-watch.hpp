// Whether a folder has anything in it (the dock's trash). A missing folder
// counts as empty and is noticed when it appears. QML's FolderListModel does
// not do: for a missing folder it silently lists another one.

#pragma once

#include <QFileSystemWatcher>
#include <QObject>

class FolderWatch : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString path READ path WRITE setPath NOTIFY pathChanged)
    Q_PROPERTY(bool empty READ isEmpty NOTIFY emptyChanged)

  public:
    explicit FolderWatch(QObject* parent = nullptr);

    QString path() const;
    void setPath(const QString& path);
    bool isEmpty() const;

  signals:
    void pathChanged();
    void emptyChanged();

  private:
    void look();

    QString watchedPath;
    bool empty = true;
    QFileSystemWatcher watcher;
};
