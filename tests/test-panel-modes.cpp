// The modes the Display pane offers (src/apps/settings/panel-modes.cpp): a
// built-in panel's smaller sizes of its own shape, and the current mode.

#include <QtTest>

#include "panel-modes.hpp"

namespace
{
bool offers(const QList<ScreenMode>& modes, int width, int height, int refresh)
{
    for (const ScreenMode& mode : modes)
    {
        if (mode.width == width && mode.height == height && mode.refresh == refresh)
        {
            return true;
        }
    }
    return false;
}

const QList<ScreenMode> PANEL = {{2560, 1600, 120000, true}, {2560, 1600, 60000, false}};
} // namespace

class TestPanelModes : public QObject
{
    Q_OBJECT

  private slots:
    void builtInPanelsGetSmallerSizesOfTheirShape()
    {
        const QList<ScreenMode> modes = offered_modes(QStringLiteral("eDP-1"), PANEL.first(), PANEL);
        QVERIFY(offers(modes, 1920, 1200, 120000));
        QVERIFY(offers(modes, 1920, 1200, 60000));
        QVERIFY(offers(modes, 1280, 800, 60000));
        QVERIFY(!offers(modes, 1920, 1080, 120000));
        QVERIFY(!offers(modes, 1024, 768, 60000));
        QVERIFY(!offers(modes, 2560, 1440, 120000));
        // Nothing larger than the panel, nor its own size twice.
        int own = 0;
        for (const ScreenMode& mode : modes)
        {
            QVERIFY(mode.width <= 2560 && mode.height <= 1600);
            own += mode.width == 2560 ? 1 : 0;
        }
        QCOMPARE(own, 2);
    }

    void uprightPanelsGetUprightSizes()
    {
        const QList<ScreenMode> upright = {{1200, 1920, 60000, true}};
        const QList<ScreenMode> modes = offered_modes(QStringLiteral("DSI-1"), upright.first(), upright);
        QVERIFY(offers(modes, 800, 1280, 60000));
        QVERIFY(!offers(modes, 1280, 800, 60000));
    }

    void aSizeChosenBeforeKeepsTheOthers()
    {
        // Once set, a stretched size is among those the panel names.
        QList<ScreenMode> after = PANEL;
        after.append({1920, 1200, 119987, false});
        const QList<ScreenMode> modes = offered_modes(QStringLiteral("eDP-1"), after.last(), after);
        QVERIFY(offers(modes, 1280, 800, 120000));
        QVERIFY(offers(modes, 1440, 900, 60000));
        QVERIFY(!offers(modes, 1920, 1200, 120000));
        QVERIFY(offers(modes, 1920, 1200, 119987));
    }

    void otherScreensKeepTheirOwnList()
    {
        QCOMPARE(offered_modes(QStringLiteral("DP-1"), PANEL.first(), PANEL).size(), PANEL.size());
        // A driver that adds sizes of its own adds other shapes too.
        const QList<ScreenMode> two_sizes = {{1920, 1080, 60000, true}, {1024, 768, 60000, false}};
        QCOMPARE(offered_modes(QStringLiteral("eDP-1"), two_sizes.first(), two_sizes).size(), two_sizes.size());
    }

    void theCurrentModeIsOffered()
    {
        // A custom mode's rate comes back a little off: not a second rate.
        const QList<ScreenMode> modes = offered_modes(QStringLiteral("eDP-1"), {1920, 1200, 119987}, PANEL);
        QVERIFY(!offers(modes, 1920, 1200, 119987));
        const QList<ScreenMode> odd = offered_modes(QStringLiteral("DP-1"), {1366, 768, 60000}, PANEL);
        QVERIFY(offers(odd, 1366, 768, 60000));
        // A virtual screen offers nothing: any size goes there.
        QVERIFY(offered_modes(QStringLiteral("HEADLESS-1"), {1280, 720, 0}, {}).isEmpty());
    }
};

QTEST_GUILESS_MAIN(TestPanelModes)
#include "test-panel-modes.moc"
