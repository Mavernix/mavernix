// Telling a shake of the pointer from ordinary movement
// (src/compositor/shake-to-find).

#include <QtTest>

#include "shake-detect.hpp"

using namespace shake_to_find;

namespace
{
// Moves `legs` times `leg_px` side to side, starting rightwards at start_ms,
// in steps of step_px every step_ms. Returns the time after the last step.
int64_t zigzag(
    shake_detect_t& detect, int legs, double leg_px, int64_t start_ms, double step_px = 10, int64_t step_ms = 8)
{
    int64_t now = start_ms;
    for (int leg = 0; leg < legs; leg++)
    {
        const double way = leg % 2 == 0 ? 1 : -1;
        for (double gone = 0; gone < leg_px; gone += step_px)
        {
            detect.move(way * step_px, 0, now);
            now += step_ms;
        }
    }
    return now;
}
} // namespace

class TestShakeToFind : public QObject
{
    Q_OBJECT

  private slots:
    void a_quick_shake_is_one()
    {
        shake_detect_t detect;
        // Five legs of 100 px, each 80 ms: four turns within 400 ms.
        const int64_t end = zigzag(detect, 5, 100, 0);
        QVERIFY(detect.shaking(end));
    }

    void three_turns_are_not_enough()
    {
        shake_detect_t detect;
        const int64_t end = zigzag(detect, 4, 100, 0);
        QVERIFY(!detect.shaking(end));
    }

    void a_slow_back_and_forth_is_no_shake()
    {
        shake_detect_t detect;
        // Legs of 400 ms: four turns take longer than a second.
        const int64_t end = zigzag(detect, 5, 100, 0, 10, 40);
        QVERIFY(!detect.shaking(end));
    }

    void small_jitter_is_no_shake()
    {
        shake_detect_t detect;
        const int64_t end = zigzag(detect, 20, TURN_AFTER_PX / 2, 0, 2);
        QVERIFY(!detect.shaking(end));
    }

    void jitter_on_the_way_adds_no_turns()
    {
        shake_detect_t detect;
        int64_t now = 0;
        // Right, a pixel back, right on, left: one turn, not three.
        for (int step = 0; step < 4; step++, now += 8)
        {
            detect.move(20, 0, now);
        }
        detect.move(-1, 0, now += 8);
        for (int step = 0; step < 4; step++, now += 8)
        {
            detect.move(20, 0, now);
        }
        // Then left, right, left make three turns, and only a fourth leg
        // the fourth.
        for (int leg = 0; leg < 4; leg++)
        {
            QVERIFY(!detect.shaking(now));
            const double way = leg % 2 == 0 ? -1 : 1;
            for (int step = 0; step < 5; step++, now += 8)
            {
                detect.move(way * 20, 0, now);
            }
        }
        QVERIFY(detect.shaking(now));
    }

    void up_and_down_is_no_shake()
    {
        shake_detect_t detect;
        int64_t now = 0;
        for (int leg = 0; leg < 8; leg++)
        {
            for (int step = 0; step < 10; step++, now += 8)
            {
                detect.move(0, leg % 2 == 0 ? 10 : -10, now);
            }
        }
        QVERIFY(!detect.shaking(now));
    }

    void the_shake_ends_soon_after_the_turns_stop()
    {
        shake_detect_t detect;
        const int64_t end = zigzag(detect, 6, 100, 0);
        QVERIFY(detect.shaking(end));
        QVERIFY(detect.shaking(end + TURNS_APART_MS / 2));
        QVERIFY(!detect.shaking(end + 2 * TURNS_APART_MS));
    }

    void moving_on_after_a_shake_does_not_restart_it_at_once()
    {
        shake_detect_t detect;
        int64_t now = zigzag(detect, 6, 100, 0);
        // Steadily one way for a second and a half, then a single turn.
        for (int step = 0; step < 150; step++, now += 10)
        {
            detect.move(-5, 0, now);
        }
        QVERIFY(!detect.shaking(now));
        now = zigzag(detect, 1, 100, now);
        QVERIFY(!detect.shaking(now));
    }

    void a_pause_forgets_the_turns()
    {
        shake_detect_t detect;
        int64_t now = zigzag(detect, 4, 100, 0);
        // Three turns, a rest, then two more legs: not four turns in a row.
        now = zigzag(detect, 2, 100, now + 2 * TURNS_APART_MS);
        QVERIFY(!detect.shaking(now));
    }

    void a_button_press_ends_it()
    {
        shake_detect_t detect;
        const int64_t end = zigzag(detect, 6, 100, 0);
        QVERIFY(detect.shaking(end));
        detect.reset();
        QVERIFY(!detect.shaking(end));
    }
};

QTEST_APPLESS_MAIN(TestShakeToFind)
#include "test-shake-to-find.moc"
