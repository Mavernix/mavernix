#include "desktop-pane.hpp"

#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QGridLayout>
#include <QIcon>
#include <QMessageBox>
#include <QPushButton>
#include <QStyle>
#include <QVBoxLayout>
#include <array>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>

#include "choices.hpp"
#include "pane-parts.hpp"
#include "picture-chooser.hpp"

// The field for the desktop's name is as wide as a short name needs.
static const int name_field_width_px = 200;

// Keep the four choices together until OK; cancel never writes preferences.
static void choose_hot_corners(Choices& choices, QWidget* parent, const QVariantMap& words)
{
    const auto word = [&words](const char* key, const char* fallback) {
        return words.value(QLatin1String(key), QString::fromUtf8(fallback)).toString();
    };
    QDialog dialog(parent);
    dialog.setWindowTitle(word("hot_corners_title", "Active Screen Corners"));
    auto* layout = new QVBoxLayout(&dialog);
    auto* grid = new QGridLayout;
    layout->addLayout(grid);
    const std::array<const char*, 4> corners = {"top_left", "top_right", "bottom_left", "bottom_right"};
    const std::array<const char*, 4> labels = {"Upper left", "Upper right", "Lower left", "Lower right"};
    const std::array<const char*, 5> actions = {"none", "window_overview", "show_desktop", "app_grid", "lock"};
    const std::array<const char*, 5> names = {"—", "Window Overview", "Desktop", "All Applications", "Lock Screen"};
    const QVariantMap selected = choices.value(QStringLiteral("hot_corners"), QStringLiteral("actions")).toMap();
    std::array<QComboBox*, 4> boxes;
    for (int i = 0; i < 4; ++i)
    {
        const QString key = QLatin1String(corners[i]);
        const QString label = words.value(QStringLiteral("hot_corners_") + key, QLatin1String(labels[i])).toString();
        auto* box = new QComboBox;
        box->setObjectName(QStringLiteral("hot_corner_") + key);
        box->setAccessibleName(label);
        for (int j = 0; j < 5; ++j)
        {
            box->addItem(words
                             .value(QStringLiteral("hot_corners_action_") + QLatin1String(actions[j]),
                                 QString::fromUtf8(names[j]))
                             .toString(),
                QLatin1String(actions[j]));
        }
        const QString current = selected.value(key, QStringLiteral("none")).toString();
        int index = box->findData(current);
        if (index < 0)
        {
            box->addItem(current, current);
            index = box->count() - 1;
        }
        box->setCurrentIndex(index);
        boxes[i] = box;
        const int row = i < 2 ? 0 : 3;
        const int column = i % 2 == 0 ? 0 : 2;
        grid->addWidget(new QLabel(label), row, column);
        grid->addWidget(box, row + 1, column);
    }
    auto* screen = new QLabel;
    const int icon_size = dialog.style()->pixelMetric(QStyle::PM_LargeIconSize) * 2;
    screen->setPixmap(
        QIcon::fromTheme(QStringLiteral("video-display"), dialog.style()->standardIcon(QStyle::SP_ComputerIcon))
            .pixmap(icon_size, icon_size));
    screen->setAlignment(Qt::AlignCenter);
    grid->addWidget(screen, 2, 0, 1, 3);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    layout->addWidget(buttons);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, [&]() {
        QVariantMap actions;
        for (int i = 0; i < 4; ++i)
        {
            actions.insert(QLatin1String(corners[i]), boxes[i]->currentData());
        }
        const QString error = choices.set(QStringLiteral("hot_corners"), QStringLiteral("actions"), actions);
        if (!error.isEmpty())
        {
            QMessageBox::warning(&dialog, dialog.windowTitle(), error);
            return;
        }
        dialog.accept();
    });
    dialog.exec();
}

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
    const QVariant translucency = choices.value(QStringLiteral("panel"), QStringLiteral("translucent_menu_bar"));
    translucent->setChecked(!translucency.isValid() || translucency.toBool());
    connect(translucent, &QCheckBox::toggled, this,
        [&choices](bool on) { choices.set(QStringLiteral("panel"), QStringLiteral("translucent_menu_bar"), on); });
    form->addRow(QString(), translucent);

    // The files of the Desktop folder as icons; the shell follows at once.
    auto* files = new QCheckBox(
        words.value(QStringLiteral("desktop_show_files"), QStringLiteral("Show the files of the Desktop folder"))
            .toString());
    files->setChecked(choices.value(QStringLiteral("desktop_files"), QStringLiteral("show")).toBool());
    connect(files, &QCheckBox::toggled, this,
        [&choices](bool on) { choices.set(QStringLiteral("desktop_files"), QStringLiteral("show"), on); });
    form->addRow(QString(), files);

    auto* corners =
        new QPushButton(words.value(QStringLiteral("hot_corners_button"), QStringLiteral("Hot Corners…")).toString());
    corners->setObjectName(QStringLiteral("hot_corners_button"));
    connect(
        corners, &QPushButton::clicked, this, [this, &choices, words]() { choose_hot_corners(choices, this, words); });
    form->addRow(QString(), corners);

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
