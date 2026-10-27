// The programs that start at login (startup-entries.hpp): a ticked list,
// with buttons to add an installed program and to remove one the user added.

#pragma once

#include <QVariantMap>
#include <QWidget>

class Choices;
class QLabel;
class QListWidget;
class QPushButton;

class StartupPane : public QWidget
{
    Q_OBJECT

  public:
    StartupPane(Choices& choices, QWidget* parent = nullptr);

  private:
    // Reads the folders again; `chosen` is the file name to mark.
    void fill(const QString& chosen = QString());
    void addProgram();
    void showProblem(const QString& problem);

    QVariantMap words;
    QListWidget* list;
    QPushButton* remove;
    QLabel* empty_label;
    QLabel* problem_label;
};
