// The Display pane's choice of resolution and refresh rate for the screen
// picked in the pane, from the modes the compositor reports
// (src/compositor/screen-modes/). A
// choice is written to [display.screens.NAME] mode and kept only if the user
// says so within a few seconds, so a mode the screen cannot show undoes
// itself. Without the compositor's answer, or for a virtual screen with no
// modes, the rows stay hidden. A built-in panel is offered smaller sizes
// too (panel-modes.hpp).

#pragma once

#include <QList>
#include <QObject>
#include <QSize>
#include <QString>
#include <QVariantMap>

#include "panel-modes.hpp"

class Choices;
class QComboBox;
class QFormLayout;
class QWidget;

struct ModedScreen
{
    QString name;
    QString description;
    ScreenMode current;
    QList<ScreenMode> modes;
};

class DisplayModes : public QObject
{
    Q_OBJECT

  public:
    DisplayModes(Choices& choices, QWidget* pane);

    // Adds the rows, hidden until the compositor has answered.
    void addRowsTo(QFormLayout* form);
    // Top to bottom, for the pane's tab order.
    QList<QWidget*> lists() const;
    // The screen whose modes the rows show.
    void showScreenNamed(const QString& name);
    // Asks the compositor again, as when a screen is plugged in.
    void ask();

  signals:
    // The screen `name` now shows this many pixels.
    void modeChanged(const QString& name, QSize pixels);

  private:
    void showScreens(const QList<ModedScreen>& found);
    void showScreen();
    // The shown screen's place in `screens`; -1 if it reported no modes.
    int shownIndex() const;
    void showRefreshRates();
    void choose();
    // The user's [display.screens.NAME] mode; empty for the screen's own.
    QString writtenMode(const QString& name) const;
    void writeMode(const QString& name, const QString& mode);
    // Asks whether to keep the mode just written; no answer in time is no.
    bool keepAsked();
    QString word(const char* key, const char* fallback) const;

    Choices& choices;
    QWidget* pane;
    QVariantMap words;
    QFormLayout* form = nullptr;
    QString shown_name;
    QComboBox* resolution_list;
    QComboBox* refresh_list;
    QList<ModedScreen> screens;
};
