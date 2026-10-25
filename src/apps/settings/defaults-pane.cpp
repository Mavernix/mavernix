#include "defaults-pane.hpp"

#include <QComboBox>
#include <QFormLayout>
#include <QLabel>
#include <QVBoxLayout>

#include <algorithm>

#include "choices.hpp"
#include "installed-programs.hpp"
#include "pane-parts.hpp"

// A group of MIME types one program is chosen for ("pictures"). The first
// type is the one asked which programs are registered; a web address opens
// as "x-scheme-handler/https".
struct KindGroup
{
    const char* name;
    const char* title;
    QStringList kinds;
};

static const QList<KindGroup>& kind_groups()
{
    static const QList<KindGroup> groups = {
        {"web",
            "Web browser:", {"x-scheme-handler/https", "x-scheme-handler/http", "text/html", "application/xhtml+xml"}},
        {"mail", "Email:", {"x-scheme-handler/mailto"}},
        {"folders", "Folders:", {"inode/directory"}},
        {"text", "Text files:", {"text/plain"}},
        {"images", "Pictures:",
            {"image/png", "image/jpeg", "image/gif", "image/webp", "image/bmp", "image/tiff", "image/svg+xml"}},
        {"documents", "PDF documents:", {"application/pdf"}},
        {"video", "Video:",
            {"video/mp4", "video/x-matroska", "video/webm", "video/quicktime", "video/x-msvideo", "video/mpeg"}},
        {"music", "Music:", {"audio/mpeg", "audio/flac", "audio/ogg", "audio/x-wav", "audio/mp4", "audio/opus"}},
    };
    return groups;
}

DefaultsPane::DefaultsPane(Choices& choices, QWidget* parent) : QWidget(parent)
{
    const QVariantMap words = choices.configTable(QStringLiteral("settings")).value(QStringLiteral("words")).toMap();
    const auto word = [&words](const QString& key, const QString& fallback) {
        return words.value(key, fallback).toString();
    };

    auto* rows = new QVBoxLayout(this);
    rows->setSpacing(14);

    auto* intro = new QLabel(word(QStringLiteral("defaults_intro"),
        QStringLiteral("Choose the program that opens each kind of file or web address.")));
    intro->setWordWrap(true);
    intro->setAlignment(Qt::AlignHCenter);
    rows->addWidget(intro);

    // Said under the choices when a choice could not be kept.
    auto* problem_label = new QLabel;
    problem_label->setWordWrap(true);
    problem_label->setAlignment(Qt::AlignHCenter);

    auto* form = new QFormLayout;
    form->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
    form->setVerticalSpacing(12);
    // A list is as wide as the longest program's name, not as the window.
    form->setFieldGrowthPolicy(QFormLayout::FieldsStayAtSizeHint);

    QList<QComboBox*> lists;
    for (const KindGroup& group : kind_groups())
    {
        const QString name = QLatin1String(group.name);
        const QStringList kinds = group.kinds;
        const QString title = word(QStringLiteral("defaults_") + name, QLatin1String(group.title));

        auto* list = new QComboBox;
        const QList<InstalledProgram> programs = programs_for_kind(kinds.first());
        const QString usual = usual_program_id(kinds.first());
        if (programs.isEmpty())
        {
            list->addItem(word(QStringLiteral("defaults_none_installed"), QStringLiteral("None installed")));
            list->setEnabled(false);
        }
        else
        {
            // If none of them is the usual one yet, the list says so
            // instead of showing the first program as if it had been chosen.
            const bool usual_is_listed = std::any_of(programs.begin(), programs.end(),
                [&usual](const InstalledProgram& program) { return program.id == usual; });
            if (!usual_is_listed)
            {
                list->addItem(word(QStringLiteral("defaults_not_set"), QStringLiteral("Not set")), QString());
            }
            for (const InstalledProgram& program : programs)
            {
                list->addItem(program.icon, program.name, program.id);
                if (program.id == usual)
                {
                    list->setCurrentIndex(list->count() - 1);
                }
            }
        }

        connect(list, &QComboBox::activated, this, [list, kinds, problem_label](int index) {
            const QString id = list->itemData(index).toString();
            if (id.isEmpty())
            {
                return;
            }
            problem_label->setText(make_usual_program(id, kinds));
        });
        form->addRow(title, list);
        lists.append(list);
    }

    // The lists are of one width, the widest one's, so that they line up.
    int widest_px = 0;
    for (const QComboBox* list : lists)
    {
        widest_px = std::max(widest_px, list->sizeHint().width());
    }
    for (QComboBox* list : lists)
    {
        list->setMinimumWidth(widest_px);
    }

    rows->addLayout(form);
    rows->addWidget(problem_label);
}
