// Dragging the edge that tiled windows share (src/compositor/shared-edges):
// which windows are on a seam, how far it may move, and where they end up.

#include <QtTest>

#include "seam-math.hpp"

using namespace shared_edges;

namespace
{
// How far apart two edges may be and still count as one seam.
const int TOLERANCE_PX = 4;
// The least width or height a window is given when it sets none itself.
const int FLOOR_PX = 80;

window_t window(int x, int y, int width, int height, int min_width = 100, int min_height = 100)
{
    window_t made;
    made.box = {x, y, width, height};
    made.min_width = min_width;
    made.min_height = min_height;
    return made;
}

bool same(const box_t& a, const box_t& b)
{
    return a.x == b.x && a.y == b.y && a.width == b.width && a.height == b.height;
}
} // namespace

class TestSharedEdges : public QObject
{
    Q_OBJECT

  private slots:
    void two_halves()
    {
        const std::vector<window_t> halves = {window(0, 0, 500, 600), window(500, 0, 500, 600)};
        const auto seam = find_seam(halves, 0, side_t::RIGHT, TOLERANCE_PX);
        QVERIFY(seam.has_value());
        QVERIFY(seam->vertical);
        QCOMPARE(seam->before.size(), std::size_t(1));
        QCOMPARE(seam->after.size(), std::size_t(1));

        // The same seam from the other side; none at the screen's edge or across.
        QVERIFY(find_seam(halves, 1, side_t::LEFT, TOLERANCE_PX).has_value());
        QVERIFY(!find_seam(halves, 0, side_t::LEFT, TOLERANCE_PX).has_value());
        QVERIFY(!find_seam(halves, 0, side_t::BOTTOM, TOLERANCE_PX).has_value());

        QVERIFY(same(moved_box(halves[0], *seam, 0, 100), {0, 0, 600, 600}));
        QVERIFY(same(moved_box(halves[1], *seam, 1, 100), {600, 0, 400, 600}));

        // Held where either window would get under its least width of 100.
        QCOMPARE(limit_move(halves, *seam, 800, FLOOR_PX), 400);
        QCOMPARE(limit_move(halves, *seam, -800, FLOOR_PX), -400);
        QCOMPARE(limit_move(halves, *seam, 37, FLOOR_PX), 37);
    }

    void one_tall_and_two_stacked()
    {
        const std::vector<window_t> three = {
            window(0, 0, 500, 600), window(500, 0, 500, 300), window(500, 300, 500, 300)};

        // However it is picked up, the seam beside the tall one moves all three.
        for (const auto& seam :
            {find_seam(three, 1, side_t::LEFT, TOLERANCE_PX), find_seam(three, 0, side_t::RIGHT, TOLERANCE_PX)})
        {
            QVERIFY(seam.has_value());
            QCOMPARE(seam->before.size(), std::size_t(1));
            QCOMPARE(seam->after.size(), std::size_t(2));
        }

        // The line between the stacked two is a seam of its own, without the tall one.
        const auto across = find_seam(three, 1, side_t::BOTTOM, TOLERANCE_PX);
        QVERIFY(across.has_value());
        QVERIFY(!across->vertical);
        QCOMPARE(across->before, std::vector<std::size_t>{1});
        QCOMPARE(across->after, std::vector<std::size_t>{2});
    }

    void windows_apart_do_not_move_together()
    {
        const std::vector<window_t> apart = {
            window(0, 0, 500, 200), window(500, 0, 500, 200), window(0, 400, 500, 200), window(500, 400, 500, 200)};
        const auto seam = find_seam(apart, 0, side_t::RIGHT, TOLERANCE_PX);
        QVERIFY(seam.has_value());
        QCOMPARE(seam->before.size(), std::size_t(1));
        QCOMPARE(seam->after.size(), std::size_t(1));
    }

    void a_small_gap_is_still_a_seam()
    {
        const std::vector<window_t> near = {window(0, 0, 500, 600), window(502, 0, 498, 600)};
        QVERIFY(find_seam(near, 0, side_t::RIGHT, TOLERANCE_PX).has_value());
        const std::vector<window_t> far = {window(0, 0, 500, 600), window(520, 0, 480, 600)};
        QVERIFY(!find_seam(far, 0, side_t::RIGHT, TOLERANCE_PX).has_value());
    }

    void windows_keep_their_own_size_limits()
    {
        std::vector<window_t> limited = {window(0, 0, 500, 600, 300), window(500, 0, 500, 600, 100)};
        const auto seam = find_seam(limited, 0, side_t::RIGHT, TOLERANCE_PX);
        QVERIFY(seam.has_value());
        QCOMPARE(limit_move(limited, *seam, -400, FLOOR_PX), -200);

        limited[1].max_width = 600;
        QCOMPARE(limit_move(limited, *seam, -400, FLOOR_PX), -100);

        limited[0].min_width = 100;
        QCOMPARE(limit_move(limited, *seam, -400, FLOOR_PX), -100);

        limited[1].max_width = 0;
        QCOMPARE(limit_move(limited, *seam, 800, FLOOR_PX), 400);
    }

    void stacked_windows_move_up_and_down()
    {
        const std::vector<window_t> stacked = {window(0, 0, 1000, 300), window(0, 300, 1000, 300)};
        const auto seam = find_seam(stacked, 1, side_t::TOP, TOLERANCE_PX);
        QVERIFY(seam.has_value());
        QVERIFY(!seam->vertical);
        QVERIFY(same(moved_box(stacked[0], *seam, 0, 50), {0, 0, 1000, 350}));
        QVERIFY(same(moved_box(stacked[1], *seam, 1, 50), {0, 350, 1000, 250}));
    }
};

QTEST_APPLESS_MAIN(TestSharedEdges)
#include "test-shared-edges.moc"
