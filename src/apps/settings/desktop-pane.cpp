#include "desktop-pane.hpp"

#include <QFormLayout>

#include "choices.hpp"
#include "picture-chooser.hpp"

DesktopPane::DesktopPane(Choices& choices, QWidget* parent) : QWidget(parent)
{
    const QVariantMap words = choices.configTable(QStringLiteral("settings")).value(QStringLiteral("words")).toMap();

    auto* form = new QFormLayout(this);
    form->setLabelAlignment(Qt::AlignRight | Qt::AlignTop);
    form->setFormAlignment(Qt::AlignHCenter | Qt::AlignTop);
    form->setVerticalSpacing(14);
    form->addRow(words.value(QStringLiteral("desktop_picture"), QStringLiteral("Picture:")).toString(),
        new PictureChooser(choices));
}
