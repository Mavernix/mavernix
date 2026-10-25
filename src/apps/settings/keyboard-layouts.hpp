// The keyboard layouts a person can choose from, and the text the config
// keeps a choice in. An input source is an XKB layout ("us") with an optional
// variant ("intl"); the list of them comes from xkeyboard-config.

#pragma once

#include <QList>
#include <QString>
#include <QStringList>

struct InputSource
{
    QString layout;
    // Empty for the layout's plain form.
    QString variant;

    bool operator==(const InputSource& other) const { return layout == other.layout && variant == other.variant; }
};

class KeyboardLayouts
{
  public:
    // Reads xkeyboard-config's rules (evdev.xml, evdev.extras.xml), skipping
    // missing files; if none could be read, `error` says what to check.
    static KeyboardLayouts read(const QStringList& files, QString* error = nullptr);

    static QStringList systemFiles();

    // Each layout plain and each of its variants, sorted by readable name.
    QList<InputSource> all() const;

    // "English (US, intl., with dead keys)"; XKB names for an unknown source.
    QString describe(const InputSource& source) const;

  private:
    struct Variant
    {
        QString name;
        QString description;
    };
    struct Layout
    {
        QString name;
        QString description;
        QList<Variant> variants;
    };

    QList<Layout> layouts;
};

// The config keeps sources as two comma lists matched by position:
// "us,de" and "intl," are American international and plain German.
QList<InputSource> parse_sources(const QString& layouts, const QString& variants);
QString layouts_text(const QList<InputSource>& sources);
QString variants_text(const QList<InputSource>& sources);
