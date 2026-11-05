// The settings window: an overview of the panes (overview.hpp) and one pane
// at a time, with back and forward and a search field, and the View and
// Window menus a Mac's System Preferences has. The config's [settings] panes
// and [settings.words] say which panes there are.

#pragma once

#include <QMainWindow>
#include <QMap>
#include <QPointer>
#include <QString>
#include <QStringList>
#include <QVariantMap>

class Choices;
class Overview;
class QAction;
class QLineEdit;
class QPixmap;
class QVariantAnimation;
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
    // With [animation.settings_page] "resize", a change of page grows or
    // shrinks the window to the new page, as a Mac's System Preferences
    // does, while the page left behind fades out over it.
    void changePage(const QPixmap& leftPage, int leftHeight);
    void endTransition();
    // Null for a name this program does not know.
    QWidget* makePane(const QString& name);
    QWidget* makeKeyboardPane();
    void makeMenus(const QVariantMap& words);

    // Restore Defaults… on a pane, and Reset All Settings… under the
    // overview: each asks first (pane-resets.hpp lists what a pane takes
    // back), then makes the panes anew, so they show the desktop's values.
    void restoreDefaults(const QString& name);
    void resetAll();
    void remakePane(const QString& name);
    bool askFirst(const QString& question, const QString& note, const QString& button);
    QString word(const char* key, const char* fallback) const;

    Choices& choices;
    QString program_name;
    // [animation.settings_page]: { kind: "resize" | "none", duration_ms }.
    QVariantMap page_motion;
    QVariantAnimation* resizing = nullptr;
    // While pages change: pictures of both over the real pages.
    QPointer<QWidget> transition;
    QStackedWidget* pages = nullptr;
    QMap<QString, int> pane_pages;
    QMap<QString, QString> pane_titles;
    // The pane in each page, to be made anew after a reset.
    QMap<QString, QPointer<QWidget>> pane_widgets;
    // [settings.words].
    QVariantMap words;

    // "" is the overview.
    void visit(const QString& name);
    // Back (-1) or forward (1).
    void stepInHistory(int by);
    void showPlace();

    Overview* overview = nullptr;
    QToolButton* back_button = nullptr;
    QToolButton* forward_button = nullptr;
    QAction* back_action = nullptr;
    QAction* forward_action = nullptr;
    QLineEdit* search = nullptr;
    QStringList history;
    int history_place = -1;
};
