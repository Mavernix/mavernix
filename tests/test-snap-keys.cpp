// What the arrow keys do to a window (src/compositor/snap-keys): every place a
// window can be in, for each of the four keys.

#include <QtTest>

#include "snap-steps.hpp"

using namespace snap_keys;

namespace
{
struct place_t
{
    const char* name;
    uint32_t edges;
};

const place_t PLACES[] = {
    {"floating", 0},
    {"whole screen", EDGES_ALL},
    {"left half", EDGE_TOP | EDGE_BOTTOM | EDGE_LEFT},
    {"right half", EDGE_TOP | EDGE_BOTTOM | EDGE_RIGHT},
    {"upper half", EDGE_TOP | EDGE_LEFT | EDGE_RIGHT},
    {"lower half", EDGE_BOTTOM | EDGE_LEFT | EDGE_RIGHT},
    {"upper left quarter", EDGE_TOP | EDGE_LEFT},
    {"upper right quarter", EDGE_TOP | EDGE_RIGHT},
    {"lower left quarter", EDGE_BOTTOM | EDGE_LEFT},
    {"lower right quarter", EDGE_BOTTOM | EDGE_RIGHT},
    // Combinations no key makes are treated as floating.
    {"left edge only", EDGE_LEFT},
    {"top and bottom only", EDGE_TOP | EDGE_BOTTOM},
};

QString name_of(uint32_t edges)
{
    for (const place_t& place : PLACES)
    {
        if (place.edges == edges)
        {
            return QString::fromLatin1(place.name);
        }
    }
    return QStringLiteral("an unnamed combination");
}

uint32_t edges_of(const QString& name)
{
    for (const place_t& place : PLACES)
    {
        if (name == QLatin1String(place.name))
        {
            return place.edges;
        }
    }
    qFatal("no place is called %s", qPrintable(name));
}

// A step in words: "nothing", "restore", "minimize" or "tile PLACE".
QString describe(const snap_step& step)
{
    switch (step.kind)
    {
    case snap_kind::NOTHING:
        return QStringLiteral("nothing");

    case snap_kind::RESTORE:
        return QStringLiteral("restore");

    case snap_kind::MINIMIZE:
        return QStringLiteral("minimize");

    case snap_kind::TILE:
        return QStringLiteral("tile ") + name_of(step.edges);
    }
    return QStringLiteral("?");
}
} // namespace

class TestSnapKeys : public QObject
{
    Q_OBJECT

  private slots:
    void arrow_key_data()
    {
        QTest::addColumn<QString>("place");
        QTest::addColumn<QStringList>("results");

        // What Left, Right, Up and Down do, in that order.
        const auto row = [](const char* place, QStringList results) {
            QTest::newRow(place) << QString::fromLatin1(place) << results;
        };
        row("floating", {"tile left half", "tile right half", "tile whole screen", "minimize"});
        row("whole screen", {"tile left half", "tile right half", "nothing", "restore"});
        row("left half", {"nothing", "restore", "tile upper left quarter", "tile lower left quarter"});
        row("right half", {"restore", "nothing", "tile upper right quarter", "tile lower right quarter"});
        row("upper half", {"tile upper left quarter", "tile upper right quarter", "tile whole screen", "restore"});
        row("lower half", {"tile lower left quarter", "tile lower right quarter", "restore", "restore"});
        row("upper left quarter", {"nothing", "tile upper right quarter", "tile whole screen", "tile left half"});
        row("upper right quarter", {"tile upper left quarter", "nothing", "tile whole screen", "tile right half"});
        row("lower left quarter", {"nothing", "tile lower right quarter", "tile left half", "restore"});
        row("lower right quarter", {"tile lower left quarter", "nothing", "tile right half", "restore"});
        row("left edge only", {"tile left half", "tile right half", "tile whole screen", "minimize"});
        row("top and bottom only", {"tile left half", "tile right half", "tile whole screen", "minimize"});
    }

    void arrow_key()
    {
        QFETCH(QString, place);
        QFETCH(QStringList, results);

        const snap_direction keys[] = {
            snap_direction::LEFT, snap_direction::RIGHT, snap_direction::UP, snap_direction::DOWN};
        for (int key = 0; key < 4; key++)
        {
            QCOMPARE(describe(next_snap_step(edges_of(place), keys[key])), results[key]);
        }
    }
};

QTEST_APPLESS_MAIN(TestSnapKeys)
#include "test-snap-keys.moc"
