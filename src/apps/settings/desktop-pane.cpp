#include "desktop-pane.hpp"

#include <QCheckBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QSlider>

#include "choices.hpp"
#include "pane-parts.hpp"
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
    // The pictures take the pane's whole width, under their label: beside a
    // label they would not fit the window.
    form->addRow(new QLabel(words.value(QStringLiteral("desktop_picture"), QStringLiteral("Picture:")).toString()));
    form->addRow(new PictureChooser(choices));

    auto* translucent = new QCheckBox(
        words.value(QStringLiteral("translucent_menu_bar"), QStringLiteral("Translucent menu bar")).toString());
    translucent->setObjectName(QStringLiteral("translucent_menu_bar"));
    const QVariant translucency = choices.value(QStringLiteral("panel"), QStringLiteral("translucent_menu_bar"));
    translucent->setChecked(translucency.toBool());
    connect(translucent, &QCheckBox::toggled, this,
        [&choices](bool on) { choices.set(QStringLiteral("panel"), QStringLiteral("translucent_menu_bar"), on); });
    form->addRow(QString(), translucent);

    // How much of the desktop shows through the translucent menu bar ([blur]
    // panel_opacity); the theme's, else opaque, until chosen.
    const QVariant chosen_opacity = choices.value(QStringLiteral("blur"), QStringLiteral("panel_opacity"));
    const double opacity =
        chosen_opacity.isValid()
            ? chosen_opacity.toDouble()
            : choices.themeTable(QStringLiteral("blur")).value(QStringLiteral("panel_opacity"), 1.0).toDouble();
    auto* transparency = stops_slider({1.0, 0.9, 0.8, 0.7, 0.6, 0.5, 0.4, 0.3}, opacity,
        [&choices](double value) { choices.set(QStringLiteral("blur"), QStringLiteral("panel_opacity"), value); });
    auto* transparency_row = with_end_words(transparency,
        words.value(QStringLiteral("menu_bar_transparency_low"), QStringLiteral("Opaque")).toString(),
        words.value(QStringLiteral("menu_bar_transparency_high"), QStringLiteral("Clear")).toString());
    transparency_row->setEnabled(translucent->isChecked());
    connect(translucent, &QCheckBox::toggled, transparency_row, &QWidget::setEnabled);
    form->addRow(words.value(QStringLiteral("menu_bar_transparency"), QStringLiteral("Transparency:")).toString(),
        transparency_row);

    // The files of the Desktop folder as icons; the shell follows at once.
    auto* files = new QCheckBox(
        words.value(QStringLiteral("desktop_show_files"), QStringLiteral("Show the files of the Desktop folder"))
            .toString());
    files->setChecked(choices.value(QStringLiteral("desktop_files"), QStringLiteral("show")).toBool());
    connect(files, &QCheckBox::toggled, this,
        [&choices](bool on) { choices.set(QStringLiteral("desktop_files"), QStringLiteral("show"), on); });
    form->addRow(QString(), files);

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
    form->addRow(words.value(QStringLiteral("desktop_name"), QStringLiteral("Name in the menu bar:")).toString(),
        with_note_under(name_field,
            words
                .value(QStringLiteral("desktop_name_note"),
                    QStringLiteral("Shown at the top left while no window is active."))
                .toString(),
            0));
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
