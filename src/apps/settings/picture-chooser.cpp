#include "picture-chooser.hpp"

#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QImageReader>
#include <QListWidget>
#include <QPushButton>
#include <QStandardPaths>
#include <QStyle>
#include <QTimer>
#include <QVBoxLayout>

#include "choices.hpp"

static const QSize THUMBNAIL_SIZE(112, 70);
static const QSize CELL_SIZE(128, 100);
// Two rows and a bit of the third, which says there is more.
static const int COLUMNS_SHOWN = 3;
static const int LIST_HEIGHT_PX = 250;
// The list wraps a little before a row is really full; this much more
// width keeps COLUMNS_SHOWN in a row (found by trying).
static const int ROW_SLACK_PX = 8;
// The value that choosing the entry writes.
static const int VALUE_ROLE = Qt::UserRole;

// Scaled to cover the size, the overhang cut off evenly; null if unreadable.
static QPixmap thumbnail_of(const QString& file, qreal pixel_ratio)
{
    QImageReader reader(file);
    reader.setAutoTransform(true);
    const QSize whole = reader.size();
    if (!whole.isValid())
    {
        return QPixmap();
    }

    // Asking the reader for the small size skips most of a large photo's decoding.
    const QSize wanted = THUMBNAIL_SIZE * pixel_ratio;
    const QSize covering = whole.scaled(wanted, Qt::KeepAspectRatioByExpanding);
    reader.setScaledSize(covering);
    const QImage image = reader.read();
    if (image.isNull())
    {
        return QPixmap();
    }

    const QRect middle((covering.width() - wanted.width()) / 2, (covering.height() - wanted.height()) / 2,
        wanted.width(), wanted.height());
    QPixmap picture = QPixmap::fromImage(image.copy(middle));
    picture.setDevicePixelRatio(pixel_ratio);
    return picture;
}

PictureChooser::PictureChooser(Choices& choices, QWidget* parent) : QWidget(parent), choices(choices)
{
    const QVariantMap words = choices.configTable(QStringLiteral("settings")).value(QStringLiteral("words")).toMap();
    const auto word = [&words](const char* key, const char* fallback) {
        return words.value(QLatin1String(key), QString::fromUtf8(fallback)).toString();
    };

    list = new QListWidget;
    list->setViewMode(QListView::IconMode);
    list->setResizeMode(QListView::Adjust);
    list->setMovement(QListView::Static);
    list->setIconSize(THUMBNAIL_SIZE);
    list->setGridSize(CELL_SIZE);
    list->setWordWrap(true);
    list->setFixedHeight(LIST_HEIGHT_PX);
    list->setMinimumWidth(COLUMNS_SHOWN * CELL_SIZE.width() + 2 * list->frameWidth() +
                          list->style()->pixelMetric(QStyle::PM_ScrollBarExtent) + ROW_SLACK_PX);
    // Shown once there is something in it (addPicture).
    list->setVisible(false);

    const QString folder_in_theme =
        choices.themeTable(QStringLiteral("desktop")).value(QStringLiteral("pictures")).toString();
    if (!folder_in_theme.isEmpty())
    {
        const QDir folder(choices.themeDirectory() + QLatin1Char('/') + folder_in_theme);
        const QStringList files = folder.entryList(
            {QStringLiteral("*.jpg"), QStringLiteral("*.jpeg"), QStringLiteral("*.png")}, QDir::Files, QDir::Name);
        for (const QString& file : files)
        {
            addPicture(folder_in_theme + QLatin1Char('/') + file);
        }
    }

    const QString chosen = choices.value(QStringLiteral("desktop"), QStringLiteral("picture")).toString();
    if (!chosen.isEmpty())
    {
        addPicture(chosen);
    }
    markChosen();

    connect(list, &QListWidget::itemClicked, this,
        [this](QListWidgetItem* item) { choose(item->data(VALUE_ROLE).toString()); });

    auto* choose_file = new QPushButton(word("desktop_picture_choose", "Choose a picture…"));
    connect(choose_file, &QPushButton::clicked, this, [this, choose_file]() {
        const QString pictures_folder = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
        const QString heading = choose_file->text().remove(QChar(0x2026));
        const QString file = QFileDialog::getOpenFileName(
            this, heading, pictures_folder, QStringLiteral("Pictures (*.jpg *.jpeg *.png *.webp *.bmp)"));
        if (!file.isEmpty())
        {
            addPicture(file);
            choose(file);
            QTimer::singleShot(0, this, &PictureChooser::loadNextThumbnail);
        }
    });

    auto* use_default = new QPushButton(word("desktop_picture_default", "Use the default"));
    connect(use_default, &QPushButton::clicked, this, [this]() { choose(QString()); });

    auto* buttons = new QHBoxLayout;
    buttons->addWidget(choose_file);
    buttons->addWidget(use_default);
    buttons->addStretch();

    auto* rows = new QVBoxLayout(this);
    rows->setContentsMargins(0, 0, 0, 0);
    rows->addWidget(list);
    rows->addLayout(buttons);

    // The pictures are read one at a time between other events, so that
    // the window is there at once and stays usable meanwhile.
    QTimer::singleShot(0, this, &PictureChooser::loadNextThumbnail);
}

void PictureChooser::addPicture(const QString& value)
{
    for (int row = 0; row < list->count(); row++)
    {
        if (list->item(row)->data(VALUE_ROLE).toString() == value)
        {
            return;
        }
    }

    auto* item = new QListWidgetItem(QFileInfo(value).completeBaseName());
    item->setData(VALUE_ROLE, value);
    item->setTextAlignment(Qt::AlignHCenter | Qt::AlignTop);
    // So that the list does not shift as the pictures come in.
    QPixmap blank(THUMBNAIL_SIZE);
    blank.fill(Qt::transparent);
    item->setIcon(blank);
    list->addItem(item);
    list->setVisible(true);
}

void PictureChooser::markChosen()
{
    QString chosen = choices.value(QStringLiteral("desktop"), QStringLiteral("picture")).toString();
    if (chosen.isEmpty())
    {
        chosen = themeBackground();
    }

    list->clearSelection();
    for (int row = 0; row < list->count(); row++)
    {
        QListWidgetItem* item = list->item(row);
        if (item->data(VALUE_ROLE).toString() == chosen)
        {
            list->setCurrentItem(item);
            list->scrollToItem(item);
        }
    }
}

void PictureChooser::choose(const QString& value)
{
    choices.set(QStringLiteral("desktop"), QStringLiteral("picture"), value);
    markChosen();
}

void PictureChooser::loadNextThumbnail()
{
    if (next_thumbnail_row >= list->count())
    {
        return;
    }

    QListWidgetItem* item = list->item(next_thumbnail_row);
    next_thumbnail_row++;
    const QPixmap picture = thumbnail_of(fileOf(item->data(VALUE_ROLE).toString()), devicePixelRatioF());
    if (!picture.isNull())
    {
        item->setIcon(picture);
    }

    QTimer::singleShot(0, this, &PictureChooser::loadNextThumbnail);
}

QString PictureChooser::fileOf(const QString& value) const
{
    if (value.startsWith(QLatin1Char('/')))
    {
        return value;
    }

    return choices.themeDirectory() + QLatin1Char('/') + value;
}

QString PictureChooser::themeBackground() const
{
    return choices.themeTable(QStringLiteral("desktop"))
        .value(QStringLiteral("background"))
        .toMap()
        .value(QStringLiteral("image"))
        .toString();
}
