// Reading a theme's window frame the way the compositor does
// (src/compositor/decoration/theme-file.cpp): the themes that come with the
// desktop are valid, and a mistake in a theme is refused with its place named.

#include <QtTest>

#include <stdexcept>

#include "theme-file.hpp"

class TestThemeFile : public QObject
{
    Q_OBJECT

  private:
    // A copy of the plain theme with `change` applied to its text, in a
    // folder of its own.
    static QString changed_plain_theme(QTemporaryDir& folder, const QString& from, const QString& to)
    {
        QFile original(QStringLiteral(THEMES_DIR "/plain/theme.toml"));
        if (!original.open(QIODevice::ReadOnly))
        {
            qFatal("cannot read the plain theme");
        }
        QString text = QString::fromUtf8(original.readAll());
        if (!text.contains(from))
        {
            qFatal("the plain theme has no \"%s\" to change", qPrintable(from));
        }
        text.replace(from, to);

        QFile changed(folder.filePath(QStringLiteral("theme.toml")));
        if (!changed.open(QIODevice::WriteOnly))
        {
            qFatal("cannot write the changed theme");
        }
        changed.write(text.toUtf8());
        return folder.path();
    }

  private slots:
    void the_shipped_themes_are_valid_data()
    {
        QTest::addColumn<QString>("theme");
        QTest::newRow("plain") << QStringLiteral("plain");
    }

    void the_shipped_themes_are_valid()
    {
        QFETCH(QString, theme);
        const themed::window_theme_t read =
            themed::load_window_theme((QStringLiteral(THEMES_DIR "/") + theme).toStdString());
        QVERIFY(read.titlebar_height > 0);
        QVERIFY(read.button_width > 0);
        QVERIFY(!read.close.looks.empty());
    }

    void a_misspelt_key_is_refused_by_name()
    {
        QTemporaryDir folder;
        const QString theme = changed_plain_theme(folder, QStringLiteral("[window.buttons.close]\n"),
            QStringLiteral("[window.buttons.close]\nnormall = { color = \"#000000\" }\n"));
        try
        {
            themed::load_window_theme(theme.toStdString());
            QFAIL("a theme with an unknown key was accepted");
        }
        catch (const std::exception& error)
        {
            QVERIFY2(QString::fromUtf8(error.what()).contains(QStringLiteral("normall")), error.what());
        }
    }

    void a_missing_theme_is_refused()
    {
        QVERIFY_THROWS_EXCEPTION(std::exception, themed::load_window_theme("/nonexistent"));
    }
};

QTEST_APPLESS_MAIN(TestThemeFile)
#include "test-theme-file.moc"
