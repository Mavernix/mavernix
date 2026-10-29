// Small parts that more than one pane is built from.

#pragma once

#include <QFont>
#include <QFontMetrics>
#include <QSize>
#include <QString>

#include <functional>

class QCheckBox;
class QSlider;
class QWidget;

// A slider with a small word under each end ("Small" ... "Large").
QWidget* with_end_words(QSlider* slider, const QString& low, const QString& high);

// Calls `apply` with the slider's value as it moves, at most once every
// `every_ms` and always with where it stops: for a setting that is felt at
// once (volume, brightness), each step would be a command of its own.
void follow_slider(QSlider* slider, int every_ms, const std::function<void(int)>& apply);

// A checkbox with a line of smaller text under it that says what it means.
QWidget* with_note(QCheckBox* box, const QString& note);

// The same for any control, the note `indent_px` in from its left edge.
QWidget* with_note_under(QWidget* control, const QString& note, int indent_px);

// Broken at spaces into lines no wider than `widest`.
QString in_lines_of(const QString& text, const QFontMetrics& metrics, int widest);

QFont scaled_font(const QFont& font, double scale);

// A pane with a list and the chosen item's details beside it, inside the
// window's 20 px margin.
inline const QSize source_list_size(232, 361);
inline const QSize detail_box_size(391, 367);
inline constexpr int list_to_box_px = 5;
inline constexpr int box_margin_px = 18;
// A little narrower than the box: a line that fits to the pixel is cut off
// by the box's own frame.
inline constexpr int box_text_width_px = 391 - 2 * 18 - 12;
// The pane's text at the top is as wide as the pane (668 less the 20 px margins).
inline constexpr int intro_width_px = 628;
// Notes are 11 pt where the rest is 13 pt.
inline constexpr double small_words_scale = 11.0 / 13.0;
inline constexpr int pane_spacing_px = 14;
