#include "dock-pane.hpp"

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QScreen>
#include <QSlider>
#include <QVBoxLayout>

#include "choices.hpp"
#include "pane-parts.hpp"

DockPane::DockPane(Choices& choices, QWidget* parent) : QWidget(parent), choices(choices)
{
    const QVariantMap words = choices.configTable(QStringLiteral("settings")).value(QStringLiteral("words")).toMap();
    const auto word = [&words](const char* key, const char* fallback) {
        return words.value(QLatin1String(key), QLatin1String(fallback)).toString();
    };

    const QVariantMap dock_looks = choices.themeTable(QStringLiteral("dock"));
    const int smallest = dock_looks.value(QStringLiteral("smallest_icon_size"), 16).toInt();
    const int largest = dock_looks.value(QStringLiteral("largest_icon_size"), 128).toInt();
    // Wide enough to be set finely.
    const int slider_width = 200;

    size = new QSlider(Qt::Horizontal);
    size->setRange(smallest, largest);
    size->setValue(currentIconSize());
    size->setMinimumWidth(slider_width);
    connect(size, &QSlider::valueChanged, this,
        [this](int value) { this->choices.set(QStringLiteral("dock"), QStringLiteral("icon_size"), value); });

    magnification = new QCheckBox(word("dock_magnification", "Enlarge under the pointer:"));
    magnification->setChecked(choices.value(QStringLiteral("dock"), QStringLiteral("magnification")).toBool());

    magnified_size = new QSlider(Qt::Horizontal);
    magnified_size->setRange(smallest, largest);
    const QVariant chosen_largest = choices.value(QStringLiteral("dock"), QStringLiteral("largest_icon_size"));
    magnified_size->setValue(chosen_largest.isValid() ? chosen_largest.toInt() : largest);
    magnified_size->setMinimumWidth(slider_width);
    magnified_size->setEnabled(magnification->isChecked());
    connect(magnified_size, &QSlider::valueChanged, this,
        [this](int value) { this->choices.set(QStringLiteral("dock"), QStringLiteral("largest_icon_size"), value); });
    connect(magnification, &QCheckBox::toggled, this, [this](bool on) {
        magnified_size->setEnabled(on);
        this->choices.set(QStringLiteral("dock"), QStringLiteral("magnification"), on);
    });

    // A double click on a title bar ([window] title_double_click): a choice
    // rather than a minimise tick box, as other desktops enlarge the window.
    auto* double_click = new QComboBox;
    const struct
    {
        const char* value;
        const char* word_key;
        const char* fallback;
    } double_click_choices[] = {
        {"none", "window_double_click_none", "do nothing"},
        {"minimize", "window_double_click_minimize", "minimize"},
        {"maximize", "window_double_click_maximize", "enlarge and restore"},
    };
    const QString chosen_double_click =
        choices.value(QStringLiteral("window"), QStringLiteral("title_double_click")).toString();
    for (const auto& choice : double_click_choices)
    {
        double_click->addItem(word(choice.word_key, choice.fallback), QLatin1String(choice.value));
        if (chosen_double_click == QLatin1String(choice.value))
        {
            double_click->setCurrentIndex(double_click->count() - 1);
        }
    }
    connect(double_click, &QComboBox::activated, this, [this, double_click](int index) {
        this->choices.set(
            QStringLiteral("window"), QStringLiteral("title_double_click"), double_click->itemData(index).toString());
    });

    auto* hides = new QCheckBox(word("dock_autohide", "Hide the dock until the pointer is at the screen's edge"));
    hides->setChecked(choices.value(QStringLiteral("dock"), QStringLiteral("autohide")).toBool());
    connect(hides, &QCheckBox::toggled, this,
        [this](bool on) { this->choices.set(QStringLiteral("dock"), QStringLiteral("autohide"), on); });

    // Labels in one column, ending where the controls begin.
    auto* form = new QFormLayout(this);
    form->setLabelAlignment(Qt::AlignRight | Qt::AlignTop);
    form->setFormAlignment(Qt::AlignHCenter | Qt::AlignTop);
    form->addRow(word("dock_size", "Size:"),
        with_end_words(size, word("dock_size_small", "Small"), word("dock_size_large", "Large")));
    form->addRow(magnification,
        with_end_words(magnified_size, word("dock_magnification_min", "Min"), word("dock_magnification_max", "Max")));
    form->addRow(word("window_double_click", "A double click on a title bar:"), double_click);
    form->addRow(QString(), hides);
}

int DockPane::currentIconSize() const
{
    const QVariant chosen = choices.value(QStringLiteral("dock"), QStringLiteral("icon_size"));
    if (chosen.isValid())
    {
        return chosen.toInt();
    }

    // The theme's size, or its smaller one on a low screen: the dock's own
    // rule (src/shell/DockLayout.js, iconSize).
    const QVariantMap looks = choices.themeTable(QStringLiteral("dock"));
    const int screen_height = QGuiApplication::primaryScreen()->size().height();
    if (looks.contains(QStringLiteral("compact_icon_size")) &&
        screen_height <= looks.value(QStringLiteral("compact_up_to_height")).toInt())
    {
        return looks.value(QStringLiteral("compact_icon_size")).toInt();
    }

    return looks.value(QStringLiteral("icon_size")).toInt();
}
