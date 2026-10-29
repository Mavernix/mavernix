// Where screens land in the Display pane's arrangement
// (src/apps/settings/screen-layout.cpp).

#include <QtTest>

#include "screen-layout.hpp"

class TestScreenLayout : public QObject
{
    Q_OBJECT

  private slots:
    void aScreenLetGoNearAnEdgeLandsOnIt()
    {
        const QRect laptop(0, 0, 1440, 900);
        // Let go a little apart from the laptop's right side and a little low.
        const QRect monitor(1500, 30, 2560, 1440);
        QCOMPARE(snapped(monitor, {laptop}, 40), QPoint(1440, 0));
        // Further down it keeps its height, as long as enough edge is shared.
        QCOMPARE(snapped(QRect(1500, 300, 2560, 1440), {laptop}, 40), QPoint(1440, 300));
        // Let go well below it, it goes under the laptop, sharing enough of its edge.
        QCOMPARE(snapped(QRect(1500, 2000, 2560, 1440), {laptop}, 40), QPoint(1440 - 100, 900));
    }

    void aScreenLetGoOnTopOfAnotherMovesBesideIt()
    {
        const QRect main(0, 0, 1920, 1080);
        const QPoint at = snapped(QRect(200, 100, 1920, 1080), {main}, 40);
        QVERIFY(!QRect(at, QSize(1920, 1080)).intersects(main));
        QVERIFY(touching(QRect(at, QSize(1920, 1080)), main));
    }

    void aScreenNeverLandsOnAThirdOne()
    {
        const QRect main(0, 0, 1920, 1080);
        const QRect right(1920, 0, 1920, 1080);
        const QPoint at = snapped(QRect(2000, 50, 1920, 1080), {main, right}, 40);
        const QRect landed(at, QSize(1920, 1080));
        QVERIFY(!landed.intersects(main));
        QVERIFY(!landed.intersects(right));
        QVERIFY(touching(landed, main) || touching(landed, right));
    }

    void aScreenThatShrankIsMovedBack()
    {
        // The left screen went from scale 1 to 2: it is half as wide, and the
        // one beside it stands off by itself.
        const QMap<QString, QRect> after{
            {QStringLiteral("DP-1"), QRect(0, 0, 1280, 720)},
            {QStringLiteral("HDMI-A-1"), QRect(2560, 0, 1920, 1080)},
        };
        const QMap<QString, QRect> tidy = tidied(after, QStringLiteral("DP-1"));
        QCOMPARE(tidy.value(QStringLiteral("DP-1")), QRect(0, 0, 1280, 720));
        QCOMPARE(tidy.value(QStringLiteral("HDMI-A-1")).topLeft(), QPoint(1280, 0));
    }

    void screensThatTouchStayWhereTheyAre()
    {
        const QMap<QString, QRect> screens{
            {QStringLiteral("eDP-1"), QRect(0, 0, 1440, 900)},
            {QStringLiteral("DP-1"), QRect(-2560, -540, 2560, 1440)},
        };
        QCOMPARE(tidied(screens, QStringLiteral("eDP-1")), screens);
    }

    void positionsCountFromTheMainScreen()
    {
        const QMap<QString, QRect> screens{
            {QStringLiteral("eDP-1"), QRect(2560, 540, 1440, 900)},
            {QStringLiteral("DP-1"), QRect(0, 0, 2560, 1440)},
        };
        const QMap<QString, QPoint> at = positions(screens, QStringLiteral("eDP-1"));
        QCOMPARE(at.value(QStringLiteral("eDP-1")), QPoint(0, 0));
        QCOMPARE(at.value(QStringLiteral("DP-1")), QPoint(-2560, -540));
    }
};

QTEST_APPLESS_MAIN(TestScreenLayout)
#include "test-screen-layout.moc"
