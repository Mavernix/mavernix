// The settings window: an overview of the panes (overview.hpp) and one pane
// at a time, with back and forward and a search field. The config's
// [settings] panes and [settings.words] say which panes there are.

#pragma once

#include <QMainWindow>
#include <QMap>
#include <QString>
#include <QStringList>

class Choices;
class Overview;
class QLineEdit;
class QStackedWidget;
class QToolButton;

class SettingsWindow : public QMainWindow
{
    Q_OBJECT

  public:
    explicit SettingsWindow(Choices& choices);

    // Does nothing for a name the window has no pane for.
    void showPane(const QString& name);
    void showOverview();

  protected:
    // A pane that grows or shrinks while shown (a row appearing later) refits the window.
    bool eventFilter(QObject* watched, QEvent* event) override;

  private:
    // The window takes the shown page's height.
    void fitHeight();
    // Null for a name this program does not know.
    QWidget* makePane(const QString& name);
    QWidget* makeKeyboardPane();

    Choices& choices;
    QString program_name;
    QStackedWidget* pages = nullptr;
    QMap<QString, int> pane_pages;
    QMap<QString, QString> pane_titles;

    // "" is the overview.
    void visit(const QString& name);
    // Back (-1) or forward (1).
    void stepInHistory(int by);
    void showPlace();

    Overview* overview = nullptr;
    QToolButton* back_button = nullptr;
    QToolButton* forward_button = nullptr;
    QLineEdit* search = nullptr;
    QStringList history;
    int history_place = -1;
};
