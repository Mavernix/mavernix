#include "desktop-pane.hpp"

#include <QFormLayout>
#include <QLineEdit>

#include "choices.hpp"
#include "picture-chooser.hpp"

// The field for the desktop's name is as wide as a short name needs.
static const int name_field_width_px = 200;

DesktopPane::DesktopPane(Choices& choices, QWidget* parent) : QWidget(parent)
{
    const QVariantMap words = choices.configTable(QStringLiteral("settings")).value(QStringLiteral("words")).toMap();

    auto* form = new QFormLayout(this);
    form->setLabelAlignment(Qt::AlignRight | Qt::AlignTop);
    form->setFormAlignment(Qt::AlignHCenter | Qt::AlignTop);
    form->setVerticalSpacing(14);
    form->addRow(words.value(QStringLiteral("desktop_picture"), QStringLiteral("Picture:")).toString(),
        new PictureChooser(choices));

    // The name the menu bar shows while no window is active, over the
    // desktop's own first menu ([desktop_menus] name). A desktop without
    // those menus has no name to change.
    const QVariant name = choices.value(QStringLiteral("desktop_menus"), QStringLiteral("name"));
    if (!name.isValid())
    {
        return;
    }
    auto* name_field = new QLineEdit(name.toString());
    name_field->setFixedWidth(name_field_width_px);
    form->addRow(
        words.value(QStringLiteral("desktop_name"), QStringLiteral("Name in the menu bar:")).toString(), name_field);
    connect(name_field, &QLineEdit::editingFinished, this, [&choices, name_field]() {
        const QString wanted = name_field->text().trimmed();
        const QString current = choices.value(QStringLiteral("desktop_menus"), QStringLiteral("name")).toString();
        // An empty name would leave the menu bar without a place to open the
        // desktop's first menu from; the field goes back instead.
        if (wanted.isEmpty())
        {
            name_field->setText(current);
            return;
        }
        if (wanted != current)
        {
            choices.set(QStringLiteral("desktop_menus"), QStringLiteral("name"), wanted);
        }
    });
}
