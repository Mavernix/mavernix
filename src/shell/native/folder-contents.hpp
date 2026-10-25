// A folder's contents, kept current. Each entry is { name, path, icon,
// genericIcon, isFolder, isLauncher }; an executable .desktop file shows its
// program's name and icon. Hidden files are left out.

#pragma once

#include <QFileSystemWatcher>
#include <QObject>
#include <QVariantList>

class FolderContents : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString path READ path WRITE setPath NOTIFY pathChanged)
    // "name", "kind" (A to Z), or "added", "modified", "created" (newest first).
    Q_PROPERTY(QString order READ order WRITE setOrder NOTIFY orderChanged)
    Q_PROPERTY(QVariantList entries READ entries NOTIFY entriesChanged)

  public:
    explicit FolderContents(QObject* parent = nullptr);

    QString path() const;
    void setPath(const QString& path);
    QString order() const;
    void setOrder(const QString& order);
    QVariantList entries() const;

  signals:
    void pathChanged();
    void orderChanged();
    void entriesChanged();

  private:
    void look();

    QString folderPath;
    QString wantedOrder = QStringLiteral("name");
    QVariantList found;
    QFileSystemWatcher watcher;
};
