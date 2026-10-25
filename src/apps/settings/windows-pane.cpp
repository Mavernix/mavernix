#include "windows-pane.hpp"

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>

#include "choices.hpp"
#include "pane-parts.hpp"

WindowsPane::WindowsPane(Choices& choices, QWidget* parent) : QWidget(parent), choices(choices)
{
    const QVariantMap words = choices.configTable(QStringLiteral("settings")).value(QStringLiteral("words")).toMap();
    const auto word = [&words](const char* key, const char* fallback) {
        return words.value(QLatin1String(key), QLatin1String(fallback)).toString();
    };
    const QString table = QStringLiteral("window");

    auto* snap = new QCheckBox(word("windows_snap", "Windows snap to the screen's sides"));
    snap->setChecked(choices.value(table, QStringLiteral("snap_to_edges")).toBool());
    connect(snap, &QCheckBox::toggled, this,
        [this, table](bool on) { this->choices.set(table, QStringLiteral("snap_to_edges"), on); });

    // The key held to move a window by dragging anywhere on it, and to
    // resize it with the secondary button; written as the compositor
    // writes keys.
    auto* drag_key = new QComboBox;
    const struct
    {
        const char* key;
        const char* word_key;
        const char* fallback;
    } key_choices[] = {
        {"", "windows_drag_key_none", "No key"},
        {"<super>", "windows_drag_key_super", "Super"},
        {"<alt>", "windows_drag_key_alt", "Alt"},
    };
    const QString chosen_key = choices.value(table, QStringLiteral("drag_key")).toString();
    for (const auto& choice : key_choices)
    {
        drag_key->addItem(word(choice.word_key, choice.fallback), QLatin1String(choice.key));
        if (chosen_key == QLatin1String(choice.key))
        {
            drag_key->setCurrentIndex(drag_key->count() - 1);
        }
    }
    connect(drag_key, &QComboBox::activated, this, [this, table, drag_key](int index) {
        this->choices.set(table, QStringLiteral("drag_key"), drag_key->itemData(index).toString());
    });

    // Labels in one column, ending where the controls begin.
    auto* form = new QFormLayout(this);
    form->setLabelAlignment(Qt::AlignRight | Qt::AlignTop);
    form->setFormAlignment(Qt::AlignHCenter | Qt::AlignTop);
    form->setVerticalSpacing(14);
    // A control is as wide as what it shows, not as the window.
    form->setFieldGrowthPolicy(QFormLayout::FieldsStayAtSizeHint);
    form->addRow(QString(),
        with_note(snap,
            word("windows_snap_note",
                "At the left or right side: that half. In a corner: that quarter. At the top: the whole screen.")));
    form->addRow(word("windows_drag_key", "Move and resize a window by holding:"), drag_key);
}
