// An application's menu, fetched over the session bus (com.canonical.dbusmenu)
// and kept up to date. Each item reaches QML as
//   { id, label, enabled, separator, checkable, checked, shortcut, children }
// with `shortcut` a list of key names such as ["Control", "S"].
// Items the application marks as invisible are left out.
//
// Usage in QML:
//   ApplicationMenu { id: menu; service: ...; path: ... }
//   menu.aboutToShow(id)  call before showing an item's submenu
//   menu.activate(id)     the user chose an item

#pragma once

#include <QDBusArgument>
#include <QObject>
#include <QTimer>
#include <QVariantList>

class ApplicationMenu : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString service READ service WRITE setService NOTIFY addressChanged)
    Q_PROPERTY(QString path READ path WRITE setPath NOTIFY addressChanged)
    Q_PROPERTY(QVariantList items READ items NOTIFY itemsChanged)

  public:
    explicit ApplicationMenu(QObject* parent = nullptr);

    QString service() const;
    void setService(const QString& service);
    QString path() const;
    void setPath(const QString& path);
    QVariantList items() const;

    Q_INVOKABLE void aboutToShow(int id);
    Q_INVOKABLE void activate(int id);

  signals:
    void addressChanged();
    void itemsChanged();

  private slots:
    // Whatever changed, the whole tree is fetched again: menus are small.
    void fetchSoon();

  private:
    void followAddress();
    void fetch();
    void setItems(const QVariantList& newItems);

    QString currentService;
    QString currentPath;
    QVariantList currentItems;

    // Several change signals often arrive together; one fetch answers all.
    QTimer fetchTimer;
    // Lets answers for an outdated address be dropped.
    int addressGeneration = 0;
};
