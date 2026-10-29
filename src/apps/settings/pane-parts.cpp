#include "pane-parts.hpp"

#include <QCheckBox>
#include <QHBoxLayout>
#include <QFontMetrics>
#include <QLabel>
#include <QStringList>
#include <QSlider>
#include <QVBoxLayout>

QFont scaled_font(const QFont& font, double scale)
{
    // A font says its size in pixels or in points, whichever it was given.
    QFont scaled = font;
    if (scaled.pixelSize() > 0)
    {
        scaled.setPixelSize(qRound(scaled.pixelSize() * scale));
    }
    else
    {
        scaled.setPointSizeF(scaled.pointSizeF() * scale);
    }
    return scaled;
}

QWidget* with_end_words(QSlider* slider, const QString& low, const QString& high)
{
    auto* box = new QWidget;
    auto* rows = new QVBoxLayout(box);
    rows->setContentsMargins(0, 0, 0, 0);
    rows->setSpacing(0);
    rows->addWidget(slider);

    auto* words = new QHBoxLayout;
    auto* low_word = new QLabel(low);
    auto* high_word = new QLabel(high);
    // The words are smaller than the pane's other text.
    const QFont small_font = scaled_font(low_word->font(), 0.8);
    low_word->setFont(small_font);
    high_word->setFont(small_font);
    words->addWidget(low_word);
    words->addStretch();
    words->addWidget(high_word);
    rows->addLayout(words);
    return box;
}

// (A wrapping label asks its layout for far more height than it fills, and
// the window, which takes its page's height, comes out twice too high.)
QString in_lines_of(const QString& text, const QFontMetrics& metrics, int widest)
{
    QStringList lines;
    QString line;
    for (const QString& word : text.split(QLatin1Char(' ')))
    {
        const QString longer = line.isEmpty() ? word : line + QLatin1Char(' ') + word;
        if (!line.isEmpty() && metrics.horizontalAdvance(longer) > widest)
        {
            lines << line;
            line = word;
        }
        else
        {
            line = longer;
        }
    }
    lines << line;
    return lines.join(QLatin1Char('\n'));
}

QWidget* with_note(QCheckBox* box, const QString& note)
{
    // The note begins under the checkbox's words, not under its box.
    const int box_width = 22;
    return with_note_under(box, note, box_width);
}

QWidget* with_note_under(QWidget* control, const QString& note, int indent_px)
{
    auto* group = new QWidget;
    auto* rows = new QVBoxLayout(group);
    rows->setContentsMargins(0, 0, 0, 0);
    rows->setSpacing(2);
    // The control keeps its own width; the note's line may be wider.
    rows->addWidget(control, 0, Qt::AlignLeft);

    if (!note.isEmpty())
    {
        auto* note_label = new QLabel(note);
        const QFont small_font = scaled_font(note_label->font(), 0.85);
        note_label->setFont(small_font);
        note_label->setEnabled(false);
        note_label->setContentsMargins(indent_px, 0, 0, 0);
        // A long note goes on in a second line rather than out of the
        // window: a note usually stands in a form's right column.
        const int widest_note = 300;
        note_label->setText(in_lines_of(note, note_label->fontMetrics(), widest_note));
        rows->addWidget(note_label);
    }

    return group;
}
