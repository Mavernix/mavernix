#include "extensions-pane.hpp"

#include <QCheckBox>
#include <QDesktopServices>
#include <QDoubleSpinBox>
#include <QDir>
#include <QFileInfo>
#include <QFrame>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QSpinBox>
#include <QStandardPaths>
#include <QUrl>
#include <QVBoxLayout>

#include <algorithm>

#include "choices.hpp"
#include "source-list-delegate.hpp"
#include "pane-parts.hpp"

// The box and the gap after it; with less, a line that just fits is cut off.
static const int tick_box_width_px = 40;
static const int detail_icon_size_px = 48;
// A setting stands in line with the words of the switch above it.
static const int setting_indent_px = 22;
// Enough for a setting like 0.125.
static const int setting_decimals = 3;

ExtensionsPane::ExtensionsPane(Choices& choices, QWidget* parent) : QWidget(parent), choices(choices)
{
    words = choices.configTable(QStringLiteral("settings")).value(QStringLiteral("words")).toMap();
    found = choices.extensions();

    auto* rows = new QVBoxLayout(this);
    rows->setContentsMargins(0, 0, 0, 0);
    rows->setSpacing(pane_spacing_px);

    auto* intro = new QLabel;
    intro->setText(
        in_lines_of(word("extensions_intro", "Extensions add features to the desktop. Switch them on or off here."),
            intro->fontMetrics(), intro_width_px));
    rows->addWidget(intro);

    if (found.isEmpty())
    {
        auto* none = new QLabel(word("extensions_none", "No extensions are installed."));
        none->setEnabled(false);
        none->setAlignment(Qt::AlignCenter);
        none->setMinimumHeight(detail_box_size.height());
        rows->addWidget(none);
    }
    else
    {
        auto* side_by_side = new QHBoxLayout;
        side_by_side->setSpacing(list_to_box_px);
        side_by_side->addWidget(buildList(), 0, Qt::AlignVCenter);
        side_by_side->addWidget(buildDetail(), 0, Qt::AlignVCenter);
        rows->addLayout(side_by_side);
        fillList(QString());
    }
    rows->addWidget(buildFolderButton(), 0, Qt::AlignLeft);
}

QString ExtensionsPane::word(const char* key, const char* fallback) const
{
    return words.value(QLatin1String(key), QString::fromUtf8(fallback)).toString();
}

QWidget* ExtensionsPane::buildList()
{
    list = new QListWidget;
    list->setFixedSize(source_list_size);
    list->setItemDelegate(new SourceListDelegate(list));
    list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    list->setIconSize(QSize(32, 32));
    connect(list, &QListWidget::currentItemChanged, this, [this](QListWidgetItem* item) {
        // A heading cannot be chosen; the arrow keys step over it.
        if (item && item->data(SourceListDelegate::kind_role).toInt() == SourceListDelegate::ItemRow)
        {
            showExtension(item->data(Qt::UserRole).toString());
        }
    });
    return list;
}

QWidget* ExtensionsPane::buildDetail()
{
    auto* box = new QGroupBox;
    box->setFixedSize(detail_box_size);
    auto* rows = new QVBoxLayout(box);
    rows->setContentsMargins(box_margin_px, box_margin_px, box_margin_px, box_margin_px);
    rows->setSpacing(pane_spacing_px);

    auto* heading = new QHBoxLayout;
    detail_icon = new QLabel;
    detail_icon->setFixedSize(detail_icon_size_px, detail_icon_size_px);
    heading->addWidget(detail_icon);
    auto* names = new QVBoxLayout;
    names->setSpacing(2);
    detail_title = new QLabel;
    QFont title_font = detail_title->font();
    title_font.setBold(true);
    detail_title->setFont(title_font);
    detail_version = new QLabel;
    detail_version->setFont(scaled_font(detail_version->font(), small_words_scale));
    detail_version->setEnabled(false);
    names->addStretch();
    names->addWidget(detail_title);
    names->addWidget(detail_version);
    names->addStretch();
    heading->addLayout(names, 1);
    rows->addLayout(heading);

    detail_description = new QLabel;
    detail_description->setFont(scaled_font(detail_description->font(), small_words_scale));
    rows->addWidget(detail_description);

    auto* divider = new QFrame;
    divider->setFrameShape(QFrame::HLine);
    divider->setFrameShadow(QFrame::Sunken);
    rows->addWidget(divider);

    detail_switch = new QCheckBox;
    connect(detail_switch, &QCheckBox::toggled, this, [this](bool on) {
        change(shown_name, on);
        // The extension moves to the other group; its settings are
        // available only while it is on.
        detail_settings->setEnabled(on);
        fillList(shown_name);
    });
    rows->addWidget(detail_switch);

    detail_settings = new QWidget;
    detail_settings_rows = new QVBoxLayout(detail_settings);
    detail_settings_rows->setContentsMargins(setting_indent_px, 0, 0, 0);
    rows->addWidget(detail_settings);
    // Without it, spare height is shared with the labels above.
    rows->addStretch(1);
    return box;
}

QWidget* ExtensionsPane::buildFolderButton()
{
    auto* button = new QPushButton(word("extensions_open_folder", "Open Extensions Folder…"));
    connect(button, &QPushButton::clicked, this, [this]() { openOwnFolder(); });
    return button;
}

QVariantMap ExtensionsPane::manifestOf(const QString& name) const
{
    return found.value(name).toMap().value(QStringLiteral("manifest")).toMap();
}

QString ExtensionsPane::titleOf(const QString& name) const
{
    return manifestOf(name).value(QStringLiteral("title"), name).toString();
}

bool ExtensionsPane::isEnabled(const QString& name) const
{
    return choices.value(QStringLiteral("extensions"), QStringLiteral("enabled")).toStringList().contains(name);
}

QIcon ExtensionsPane::iconOf(const QString& name) const
{
    // `icon` in its extension.toml; a path leading out of the folder is not followed.
    const QString file = manifestOf(name).value(QStringLiteral("icon")).toString();
    if (!file.isEmpty())
    {
        const QString directory =
            QFileInfo(found.value(name).toMap().value(QStringLiteral("directory")).toString()).canonicalFilePath();
        const QString path = QFileInfo(QDir(directory).filePath(file)).canonicalFilePath();
        if (!directory.isEmpty() && path.startsWith(directory + QLatin1Char('/')))
        {
            return QIcon(path);
        }
    }
    return QIcon::fromTheme(
        QStringLiteral("preferences-plugin"), QIcon::fromTheme(QStringLiteral("preferences-system")));
}

void ExtensionsPane::fillList(const QString& chosen_name)
{
    QStringList turned_on;
    QStringList turned_off;
    for (auto extension = found.constBegin(); extension != found.constEnd(); ++extension)
    {
        (isEnabled(extension.key()) ? turned_on : turned_off).append(extension.key());
    }
    auto by_title = [this](const QString& first, const QString& second) {
        return titleOf(first).compare(titleOf(second), Qt::CaseInsensitive) < 0;
    };
    std::sort(turned_on.begin(), turned_on.end(), by_title);
    std::sort(turned_off.begin(), turned_off.end(), by_title);

    // No current-row signals while refilling.
    const QSignalBlocker quiet(list);
    list->clear();
    QListWidgetItem* to_choose = nullptr;
    auto add_group = [&](const QString& heading, const QStringList& names, bool on) {
        if (names.isEmpty())
        {
            return;
        }
        auto* heading_item = new QListWidgetItem(heading, list);
        heading_item->setFlags(Qt::NoItemFlags);
        heading_item->setData(SourceListDelegate::kind_role, SourceListDelegate::GroupHeadingRow);
        for (const QString& name : names)
        {
            auto* item = new QListWidgetItem(iconOf(name), titleOf(name), list);
            item->setData(Qt::UserRole, name);
            item->setData(SourceListDelegate::kind_role, SourceListDelegate::ItemRow);
            // The line under the title: the version, else whether it is on.
            const QString version = manifestOf(name).value(QStringLiteral("version")).toString();
            item->setData(SourceListDelegate::detail_role,
                version.isEmpty() ? (on ? word("extensions_state_on", "On") : word("extensions_state_off", "Off"))
                                  : word("extensions_version", "Version %1").arg(version));
            if (!to_choose || name == chosen_name)
            {
                to_choose = item;
            }
        }
    };
    add_group(word("extensions_group_on", "Turned on"), turned_on, true);
    add_group(word("extensions_group_off", "Turned off"), turned_off, false);

    list->setCurrentItem(to_choose);
    showExtension(to_choose->data(Qt::UserRole).toString());
}

void ExtensionsPane::showExtension(const QString& name)
{
    shown_name = name;
    const QVariantMap manifest = manifestOf(name);
    const QString title = titleOf(name);

    detail_icon->setPixmap(iconOf(name).pixmap(QSize(detail_icon_size_px, detail_icon_size_px), devicePixelRatioF()));
    detail_title->setText(title);
    const QString version = manifest.value(QStringLiteral("version")).toString();
    detail_version->setText(version.isEmpty() ? QString() : word("extensions_version", "Version %1").arg(version));
    detail_description->setText(in_lines_of(manifest.value(QStringLiteral("description")).toString(),
        detail_description->fontMetrics(), box_text_width_px));

    const bool on = isEnabled(name);
    const QSignalBlocker quiet(detail_switch);
    detail_switch->setText(word("extensions_switch", "Turn on %1").arg(title));
    detail_switch->setChecked(on);

    // A tick box for each yes-or-no setting with a label ([settings_labels]
    // in its extension.toml).
    while (QLayoutItem* old = detail_settings_rows->takeAt(0))
    {
        old->widget()->deleteLater();
        delete old;
    }
    const QVariantMap settings_now = found.value(name).toMap().value(QStringLiteral("settings")).toMap();
    const QVariantMap chosen = choices.value(QStringLiteral("extensions"), name).toMap();
    const QVariantMap labels = manifest.value(QStringLiteral("settings_labels")).toMap();
    const QStringList decimal_settings =
        found.value(name).toMap().value(QStringLiteral("decimal_settings")).toStringList();
    for (auto label = labels.constBegin(); label != labels.constEnd(); ++label)
    {
        const QString key = label.key();
        const QVariant value = chosen.contains(key) ? chosen.value(key) : settings_now.value(key);
        // A number or a text: a field with its label above it.
        if (value.typeId() != QMetaType::Bool)
        {
            QWidget* field = nullptr;
            // JSON readers take 1.0 for 1, so whether a number has decimals
            // comes from its default in extension.toml.
            const bool number_type = value.typeId() == QMetaType::Double || value.typeId() == QMetaType::Int ||
                                     value.typeId() == QMetaType::LongLong;
            if (number_type && decimal_settings.contains(key))
            {
                auto* number = new QDoubleSpinBox;
                number->setRange(-1e6, 1e6);
                number->setDecimals(setting_decimals);
                number->setValue(value.toDouble());
                // Leaving the field also ends editing; an untouched value is not written back.
                connect(number, &QDoubleSpinBox::editingFinished, this,
                    [this, name, key, number, was = number->value()]() mutable {
                    if (number->value() != was)
                    {
                        was = number->value();
                        changeSetting(name, key, was);
                    }
                });
                field = number;
            }
            else if (number_type)
            {
                auto* number = new QSpinBox;
                number->setRange(-1000000, 1000000);
                number->setValue(value.toInt());
                connect(number, &QSpinBox::editingFinished, this,
                    [this, name, key, number, was = number->value()]() mutable {
                    if (number->value() != was)
                    {
                        was = number->value();
                        changeSetting(name, key, was);
                    }
                });
                field = number;
            }
            else if (value.typeId() == QMetaType::QString)
            {
                auto* text = new QLineEdit(value.toString());
                connect(text, &QLineEdit::editingFinished, this,
                    [this, name, key, text]() { changeSetting(name, key, text->text()); });
                field = text;
            }
            if (field == nullptr)
            {
                // Lists and tables stay in the extension's own file.
                continue;
            }
            auto* labelled = new QWidget;
            auto* labelled_rows = new QVBoxLayout(labelled);
            labelled_rows->setContentsMargins(0, 0, 0, 0);
            labelled_rows->setSpacing(2);
            labelled_rows->addWidget(new QLabel(label.value().toString()));
            labelled_rows->addWidget(field, 0, Qt::AlignLeft);
            detail_settings_rows->addWidget(labelled);
            continue;
        }
        // A tick box does not wrap its words by itself.
        auto* setting = new QCheckBox;
        setting->setText(in_lines_of(label.value().toString(), setting->fontMetrics(),
            box_text_width_px - setting_indent_px - tick_box_width_px));
        setting->setChecked(value.toBool());
        connect(setting, &QCheckBox::toggled, this,
            [this, name, key](bool setting_on) { changeSetting(name, key, setting_on); });
        detail_settings_rows->addWidget(setting);
    }
    detail_settings->setEnabled(on);
}

void ExtensionsPane::changeSetting(const QString& name, const QString& key, const QVariant& value)
{
    QVariantMap chosen = choices.value(QStringLiteral("extensions"), name).toMap();
    chosen.insert(key, value);
    choices.set(QStringLiteral("extensions"), name, chosen);
}

void ExtensionsPane::change(const QString& name, bool on)
{
    QStringList enabled = choices.value(QStringLiteral("extensions"), QStringLiteral("enabled")).toStringList();
    enabled.removeAll(name);
    if (on)
    {
        enabled.append(name);
    }
    choices.set(QStringLiteral("extensions"), QStringLiteral("enabled"), enabled);
}

void ExtensionsPane::openOwnFolder() const
{
    // Where make_shell_config.py looks for the user's extensions first;
    // made if missing, so that there is a folder to open.
    const QString folder = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) +
                           QStringLiteral("/mavernix/extensions");
    QDir().mkpath(folder);
    QDesktopServices::openUrl(QUrl::fromLocalFile(folder));
}
