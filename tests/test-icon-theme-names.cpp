// The icon names the icon chooser offers (src/apps/settings/icon-theme-names.cpp):
// the current theme's own first, then those it inherits.

#include <QIcon>
#include <QTemporaryDir>
#include <QtTest>

#include "icon-theme-names.hpp"

namespace
{
// A theme folder with an index listing `folders`, and an empty file for
// every "folder/name.png" in `icons`.
void write_theme(const QString& root, const QString& theme, const QString& inherits, const QStringList& folders,
    const QStringList& icons)
{
    const QDir theme_folder(root + QLatin1Char('/') + theme);
    QVERIFY(theme_folder.mkpath(QStringLiteral(".")));
    QFile index(theme_folder.filePath(QStringLiteral("index.theme")));
    QVERIFY(index.open(QIODevice::WriteOnly));
    QByteArray text = "[Icon Theme]\nName=" + theme.toUtf8() + "\n";
    if (!inherits.isEmpty())
    {
        text += "Inherits=" + inherits.toUtf8() + "\n";
    }
    text += "Directories=" + folders.join(QLatin1Char(',')).toUtf8() + "\n";
    for (const QString& folder : folders)
    {
        text += "\n[" + folder.toUtf8() + "]\nSize=48\nType=Fixed\n";
    }
    index.write(text);
    for (const QString& icon : icons)
    {
        QVERIFY(theme_folder.mkpath(QFileInfo(icon).path()));
        QFile file(theme_folder.filePath(icon));
        QVERIFY(file.open(QIODevice::WriteOnly));
    }
}
} // namespace

class TestIconThemeNames : public QObject
{
    Q_OBJECT

  private slots:
    void theThemesOwnIconsComeBeforeInheritedOnes()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        write_theme(root.path(), QStringLiteral("SeaLion"), QStringLiteral("breeze,hicolor"),
            {QStringLiteral("48x48/apps")},
            {QStringLiteral("48x48/apps/web-browser.png"), QStringLiteral("48x48/apps/folder.png"),
                QStringLiteral("48x48/apps/go-home-symbolic.png"), QStringLiteral("unlisted/zebra.png")});
        write_theme(root.path(), QStringLiteral("breeze"), QString(), {QStringLiteral("48x48/apps")},
            {QStringLiteral("48x48/apps/akregator.png"), QStringLiteral("48x48/apps/web-browser.png"),
                QStringLiteral("48x48/apps/Zoom.png")});
        write_theme(root.path(), QStringLiteral("hicolor"), QString(), {QStringLiteral("48x48/apps")},
            {QStringLiteral("48x48/apps/aardvark.png")});

        QIcon::setThemeSearchPaths({root.path()});
        QIcon::setThemeName(QStringLiteral("SeaLion"));
        QIcon::setFallbackThemeName(QString());

        // Each theme's sorted, a name only where it is first found; neither
        // one-colour icons nor folders the index leaves out.
        QCOMPARE(
            icon_theme_names(), QStringList({QStringLiteral("folder"), QStringLiteral("web-browser"),
                                    QStringLiteral("akregator"), QStringLiteral("Zoom"), QStringLiteral("aardvark")}));
    }
};

QTEST_GUILESS_MAIN(TestIconThemeNames)
#include "test-icon-theme-names.moc"
