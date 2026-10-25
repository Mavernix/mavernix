// Owns com.canonical.AppMenu.Registrar, which makes Qt applications export
// their menu bar. On Wayland the address goes to the compositor, so owning
// the name is all that is needed.

#pragma once

#include <QObject>

class MenuRegistrar : public QObject
{
    Q_OBJECT
    // False if another program already owns the name, or there is no bus.
    Q_PROPERTY(bool registered READ isRegistered CONSTANT)

  public:
    explicit MenuRegistrar(QObject* parent = nullptr);
    ~MenuRegistrar() override;

    bool isRegistered() const;

  private:
    bool registered = false;
};
