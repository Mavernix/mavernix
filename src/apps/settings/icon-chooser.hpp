// A window for choosing an application's icon from the icon theme, with a
// filter by name and a button to take a picture file instead.

#pragma once

#include <QDialog>
#include <QVariantMap>

class QDialogButtonBox;
class QLineEdit;
class QListView;
class QSortFilterProxyModel;

class IconChooser : public QDialog
{
    Q_OBJECT

  public:
    enum Outcome
    {
        Cancelled,
        // The name is in `chosen_name`.
        IconChosen,
        // The pane then asks for the file.
        FileWanted,
    };

    // `current_name` is preselected; empty for none.
    static Outcome ask(QWidget* parent, const QVariantMap& words, const QString& heading, const QString& current_name,
        QString& chosen_name);

  private:
    IconChooser(QWidget* parent, const QVariantMap& words, const QString& heading);
    // Does nothing if the name is empty or filtered away.
    void choose(const QString& name);
    QString chosenName() const;

    QLineEdit* search;
    QListView* grid;
    QSortFilterProxyModel* matching;
    QDialogButtonBox* buttons;
};
