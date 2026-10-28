#include "toolkits-pane.hpp"

#include <QCheckBox>
#include <QFormLayout>

#include "choices.hpp"
#include "pane-parts.hpp"

static const QString gtk_table = QStringLiteral("gtk");
static const QString qt_table = QStringLiteral("qt");
// A checkbox for a toolkit's "use_theme" key.
static QCheckBox* use_theme_box(Choices& choices, const QString& table, const QString& words)
{
    auto* box = new QCheckBox(words);
    box->setChecked(choices.value(table, QStringLiteral("use_theme")).toBool());
    QObject::connect(box, &QCheckBox::toggled, box,
        [&choices, table](bool on) { choices.set(table, QStringLiteral("use_theme"), on); });
    return box;
}

ToolkitsPane::ToolkitsPane(Choices& choices, QWidget* parent) : QWidget(parent), choices(choices)
{
    const QVariantMap words = choices.configTable(QStringLiteral("settings")).value(QStringLiteral("words")).toMap();
    const auto word = [&words](const char* key, const char* fallback) {
        return words.value(QLatin1String(key), QLatin1String(fallback)).toString();
    };

    auto* gtk_theme = use_theme_box(choices, gtk_table, word("toolkits_gtk_theme", "Drawn in the desktop's look"));

    auto* qt_theme = use_theme_box(choices, qt_table, word("toolkits_qt_theme", "Drawn in the desktop's look"));

    // Labels in one column, ending where the controls begin.
    auto* form = new QFormLayout(this);
    form->setLabelAlignment(Qt::AlignRight | Qt::AlignTop);
    form->setFormAlignment(Qt::AlignHCenter | Qt::AlignTop);
    form->setVerticalSpacing(14);
    // A control is as wide as what it shows, not as the window.
    form->setFieldGrowthPolicy(QFormLayout::FieldsStayAtSizeHint);
    form->addRow(word("toolkits_gtk", "GTK programs:"), with_note(gtk_theme, word("toolkits_gtk_theme_note", "")));
    form->addRow(word("toolkits_qt", "Qt programs:"), with_note(qt_theme, word("toolkits_qt_theme_note", "")));
}
