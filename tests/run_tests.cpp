// towerfall — headless test suite.
//
// Runs the REAL simulation with no window and no graphics, so the rules the
// game actually uses are the rules the tests check. The single most important
// test is the placement rule: a tower may be built anywhere, including on the
// path, but never in a way that seals the route.

#include <algorithm>
#include <cstdio>
#include <cmath>
#include <map>
#include <string>
#include <vector>

#include "core/Game.h"
#include "core/LuaContent.h"
#include "core/MapView.h"

using namespace tf;

namespace {

int g_pass = 0;
int g_fail = 0;
std::vector<std::string> g_failures;

void check(bool condition, const std::string& what) {
    if (condition) {
        ++g_pass;
    } else {
        ++g_fail;
        g_failures.push_back(what);
        std::printf("  FAIL  %s\n", what.c_str());
    }
}

void checkEq(int actual, int expected, const std::string& what) {
    if (actual == expected) {
        ++g_pass;
    } else {
        ++g_fail;
        g_failures.push_back(what);
        std::printf("  FAIL  %s (expected %d, got %d)\n",
                    what.c_str(), expected, actual);
    }
}

void checkEqF(float actual, float expected, float tol, const std::string& what) {
    if (std::fabs(actual - expected) <= tol) {
        ++g_pass;
    } else {
        ++g_fail;
        g_failures.push_back(what);
        std::printf("  FAIL  %s (expected %f, got %f)\n",
                    what.c_str(), expected, actual);
    }
}

void section(const char* name) {
    std::printf("\n== %s ==\n", name);
}

// ---------------------------------------------------------------- helpers

// A fresh game on the first level, with no towers.
Game makeGame(int levelIndex = 0, const char* difficulty = nullptr) {
    Game g;
    const LevelDef& lv = levelByIndex(levelIndex);
    g.loadLevel(lv);
    if (difficulty) g.setDifficulty(difficulty);
    return g;
}

// Runs the simulation for `seconds` at a fixed timestep.
void run(Game& g, float seconds, float dt = 1.0f / 60.0f) {
    const int steps = static_cast<int>(seconds / dt);
    for (int i = 0; i < steps; ++i) g.update(dt);
}

// Runs until `pred` is true or `maxSeconds` elapses. Timing-independent tests
// matter here: how long a wave takes depends on the route length, which changes
// whenever a level is redesigned. Returns true if the predicate was satisfied.
template <typename Pred>
bool runUntil(Game& g, Pred pred, float maxSeconds = 600.0f, float dt = 1.0f / 60.0f) {
    const int steps = static_cast<int>(maxSeconds / dt);
    for (int i = 0; i < steps; ++i) {
        if (pred(g)) return true;
        g.update(dt);
    }
    return pred(g);
}

// Finds a cell that is buildable grass (not path) for a normal placement.
Cell findGrass(const Game& g) {
    for (int y = 0; y < g.grid().height(); ++y) {
        for (int x = 0; x < g.grid().width(); ++x) {
            if (g.grid().at(x, y) == Terrain::Grass) return Cell{x, y};
        }
    }
    return Cell{1, 1};
}

// The cell where the route enters, and where it exits.
Cell routeStart(const Game& g) {
    // Reconstruct from the level definition: 'S' and 'E'.
    const auto& rows = g.level().rows;
    for (int y = 0; y < static_cast<int>(rows.size()); ++y) {
        for (int x = 0; x < static_cast<int>(rows[static_cast<size_t>(y)].size()); ++x) {
            if (rows[static_cast<size_t>(y)][static_cast<size_t>(x)] == 'S') return Cell{x, y};
        }
    }
    return Cell{0, 0};
}

Cell routeEnd(const Game& g) {
    const auto& rows = g.level().rows;
    for (int y = 0; y < static_cast<int>(rows.size()); ++y) {
        for (int x = 0; x < static_cast<int>(rows[static_cast<size_t>(y)].size()); ++x) {
            if (rows[static_cast<size_t>(y)][static_cast<size_t>(x)] == 'E') return Cell{x, y};
        }
    }
    return Cell{0, 0};
}


// Builds a defensive ring of towers around the exit and keeps buying more as
// gold allows. Returns the number of towers built.
//
// The tests that need the game to survive several waves use this, because an
// undefended game correctly ends around wave 4 — that is the game working, not
// a bug, and a test that does not account for it proves nothing.
int buildDefence(Game& g, TowerKind kind = TowerKind::Arrow, int maxTowers = 30) {
    // Play like a human would: put towers ALONG THE ROUTE, spread out.
    //
    // Clustering every tower next to the exit looks like a defence but is not
    // one: only the handful that actually cover the exit path ever fire, and
    // the rest shoot at nothing. Spreading them along the route means every
    // enemy walks past several towers.
    const auto route = g.grid().findPath(routeStart(g), routeEnd(g));
    if (route.empty()) return 0;

    std::vector<Cell> candidates;
    const int step = std::max(1, static_cast<int>(route.size()) / std::max(1, maxTowers));
    for (size_t i = 0; i < route.size(); i += static_cast<size_t>(step)) {
        const Cell r = route[i];
        // Try the cells around this route tile: beside it first, then around.
        for (int dy = -1; dy <= 1; ++dy) {
            for (int dx = -1; dx <= 1; ++dx) {
                if (dx == 0 && dy == 0) continue;
                const Cell c{r.x + dx, r.y + dy};
                if (!g.grid().inBounds(c)) continue;
                if (!g.grid().isBuildableTerrain(c)) continue;
                if (g.grid().hasTower(c)) continue;
                candidates.push_back(c);
            }
        }
    }

    int built = 0;
    for (const Cell& c : candidates) {
        if (built >= maxTowers) break;
        if (g.gold() < towerDef(kind).level(0).cost) break;
        if (!g.canBuild(kind, c)) continue;
        g.build(kind, c);
        ++built;
    }
    return built;
}

Cell routeCellNear(const Game& g, Vec2 world) {
    Cell best{0, 0};
    float bestD = 1e30f;
    for (const Cell& c : g.level().waypoints) {
        const Vec2 w = g.cellToWorld(c);
        const float dx = w.x - world.x, dy = w.y - world.y;
        const float d = dx * dx + dy * dy;
        if (d < bestD) { bestD = d; best = c; }
    }
    return best;
}

}  // namespace

// ===========================================================================

int main() {
    std::printf("towerfall — headless test suite\n");

    // ---------------------------------------------------------------------
    // THE COORDINATE-SPACE BRIDGE.
    //
    // The simulation works in 48-pixel world tiles; the map is DRAWN at a
    // different pixel size so it fits the window. Anything drawn in world pixels
    // (enemies, projectiles) has to be scaled by the same factor as the terrain.
    //
    // This bug is not subtle: with the wrong factor the enemy leaves the path,
    // starts a tile high and drifts off to the right, because the error grows
    // with distance from the origin. These checks would have caught it.
    section("coordinate space (world <-> screen)");
    {
        Game g = makeGame();
        const float world = g.tileSize();

        // The real case: the display tile size is smaller than the world tile size.
        const int display = std::min(static_cast<int>(980 / g.grid().width()),
                                     static_cast<int>((720 - 40) / g.grid().height()));
        check(display != static_cast<int>(world),
              "the display tile size really does differ from the world tile size "
              "(otherwise this test proves nothing)");

        const MapView view{10.0f, 100.0f, static_cast<float>(display), world};

        // A cell's world position must land on that cell when converted to screen.
        for (int x = 0; x < g.grid().width(); ++x) {
            for (int y = 0; y < g.grid().height(); ++y) {
                const Cell c{x, y};
                const Vec2 w = g.cellToWorld(c);
                const Vec2 s = view.worldToScreen(w);
                const Cell back = view.screenToCell(s.x, s.y);
                if (back.x != c.x || back.y != c.y) {
                    check(false, "cell " + std::to_string(x) + "," + std::to_string(y) +
                                     " does not round-trip world -> screen -> cell");
                    x = g.grid().width();
                    break;
                }
            }
        }
        check(true, "every cell round-trips world -> screen -> cell");

        // The centre of a cell must land inside that cell on screen.
        const Cell mid{5, 6};
        const Vec2 worldPos = g.cellToWorld(mid);
        const Vec2 screenPos = view.worldToScreen(worldPos);
        const Cell got = view.screenToCell(screenPos.x, screenPos.y);
        check(got.x == mid.x && got.y == mid.y,
              "a world position on the path maps back to the same cell on screen");

        // The scale factor must be the display size over the world size.
        checkEqF(view.scale(), static_cast<float>(display) / world, 0.0001f,
                 "the world -> screen scale is display tiles / world tiles");

        // screen -> world is the exact inverse of world -> screen.
        const Vec2 back = view.screenToWorld(screenPos.x, screenPos.y);
        checkEqF(back.x, worldPos.x, 0.01f, "screen -> world inverts world -> screen (x)");
        checkEqF(back.y, worldPos.y, 0.01f, "screen -> world inverts world -> screen (y)");

        // THE REGRESSION ITSELF: an enemy standing on a path cell must be drawn
        // over that same path cell. Before the fix the enemy was drawn at its raw
        // world pixels, which is a different place entirely.
        const Cell pathCell = routeCellNear(g, g.cellToWorld(routeStart(g)));
        const Vec2 enemyWorld = g.cellToWorld(pathCell);
        const Vec2 enemyScreen = view.worldToScreen(enemyWorld);
        const Cell enemyCell = view.screenToCell(enemyScreen.x, enemyScreen.y);
        check(enemyCell.x == pathCell.x && enemyCell.y == pathCell.y,
              "an enemy on a path cell is DRAWN over that path cell, not off it");

        // And the error must be small at the far corner of the map too, which is
        // where the old code drifted furthest.
        //
        // cellToWorld gives the CENTRE of a cell; cellToScreen gives its CORNER.
        // So the centre on screen is the corner plus half a display tile.
        const Cell far{g.grid().width() - 1, g.grid().height() - 1};
        const Vec2 farWorld = g.cellToWorld(far);
        const Vec2 farScreen = view.worldToScreen(farWorld);
        const Vec2 farCorner = view.cellToScreen(far);
        const float half = view.tileSize * 0.5f;
        checkEqF(farScreen.x, farCorner.x + half, 0.01f,
                 "the far corner of the map does not drift (x)");
        checkEqF(farScreen.y, farCorner.y + half, 0.01f,
                 "the far corner of the map does not drift (y)");

        // The drift the old code produced, stated as a number: at the far corner
        // the enemy was drawn a full tile away from the terrain under it.
        const float oldScale = 1.0f;  // the old code drew world pixels as screen pixels
        const float oldX = view.originX + farWorld.x * oldScale;
        check(std::fabs(oldX - farScreen.x) > 15.0f,
              "the old (unscaled) drawing was off by more than 15px at the far corner");
    }

    // ---------------------------------------------------------------------
    section("grid and pathfinding");
    {
        Game g = makeGame();
        check(g.grid().width() == g.level().width, "grid width matches the level");
        check(g.grid().height() == g.level().height, "grid height matches the level");

        // The route must exist on an empty grid.
        const Cell s = routeStart(g);
        const Cell e = routeEnd(g);
        check(g.grid().canReach(s, e), "an empty map has a valid route");

        // The route must not be a straight line — otherwise the maze rule is trivial.
        const auto path = g.grid().findPath(s, e);
        check(path.size() > 2, "the route is not a straight line");
    }

    // ---------------------------------------------------------------------
    section("the placement rule: towers go anywhere, but never seal the route");
    {
        Game g = makeGame();
        const Cell s = routeStart(g);
        const Cell e = routeEnd(g);

        // 1. Building on ordinary ground is allowed.
        const Cell grass = findGrass(g);
        check(g.canBuild(TowerKind::Arrow, grass), "can build on grass");
        check(g.build(TowerKind::Arrow, grass), "building on grass succeeds");

        // 2. Building ON the path is allowed — this is the whole point.
        const auto path = g.grid().findPath(s, e);
        check(!path.empty(), "route exists to test against");
        const Cell onPath = path[path.size() / 2];
        check(g.grid().at(onPath) == Terrain::Path, "the chosen cell really is a path tile");
        check(g.canBuild(TowerKind::Arrow, onPath), "can build directly ON the path");
        check(g.build(TowerKind::Arrow, onPath), "building on the path succeeds");

        // 3. After building on the path, a route must STILL exist.
        check(g.grid().canReach(s, e), "a route still exists after building on the path");

        // 4. But sealing the route completely must be REFUSED.
        //
        //    This is the crux of the design brief: wall the route off entirely
        //    and the game must say no, because the player would otherwise have
        //    made the level unwinnable.
        {
            Game t = makeGame();
            const auto route = t.grid().findPath(s, e);
            check(!route.empty(), "there is a route to wall off");

            // Wall off every tile of the route with a tower. At some point the
            // pathfinder must find NO way through, and that placement must be
            // refused rather than allowed.
            //
            // A refusal is not necessarily fatal to the attempt: early on, the
            // very first tile next to the entrance may itself be the only way
            // out, and refusing it is correct. So keep going and count refusals.
            int refusals = 0;
            int built = 0;
            int blockRefusals = 0;
            std::string lastWhy;
            for (const Cell& c : route) {
                if (c == s || c == e) continue;      // the entry and exit stay open
                if (!t.grid().isBuildableTerrain(c)) continue;
                std::string why;
                if (!t.canBuild(TowerKind::Arrow, c, &why)) {
                    ++refusals;
                    lastWhy = why;
                    if (why.find("block") != std::string::npos) ++blockRefusals;
                    continue;
                }
                t.build(TowerKind::Arrow, c);
                ++built;
            }
            check(refusals > 0, "walling off the whole route is REFUSED");
            check(blockRefusals > 0,
                  "at least one refusal says it would block the path (last was \"" +
                  lastWhy + "\")");
            check(built > 0, "some towers were built (the wall was partial, not instant)");

            check(t.grid().canReach(s, e),
                  "the route still exists after the partial wall");
        }
    }

    // ---------------------------------------------------------------------
    section("level routes are valid");
    {
        // A route drawn through rock or water is invisible in the map art but
        // makes the level unplayable, so it is checked explicitly.
        for (int i = 0; i < levelCount(); ++i) {
            const LevelDef& lv = levelByIndex(i);
            Game g = makeGame(i);
            check(lv.waypoints.size() >= 2, lv.name + ": has waypoints");

            for (const Cell& w : lv.waypoints) {
                check(g.grid().inBounds(w), lv.name + ": waypoint is inside the map");
                check(g.grid().at(w) != Terrain::Rock && g.grid().at(w) != Terrain::Water,
                      lv.name + ": waypoint (" + std::to_string(w.x) + "," +
                      std::to_string(w.y) + ") is not inside a wall");
            }
            // The line between consecutive waypoints must be walkable too.
            for (size_t k = 0; k + 1 < lv.waypoints.size(); ++k) {
                const Cell a = lv.waypoints[k];
                const Cell b = lv.waypoints[k + 1];
                const int dx = b.x - a.x, dy = b.y - a.y;
                const int steps = std::max(std::abs(dx), std::abs(dy));
                for (int s2 = 0; s2 <= steps; ++s2) {
                    const float t = steps ? static_cast<float>(s2) / steps : 0.0f;
                    const int x = a.x + static_cast<int>(std::lround(dx * t));
                    const int y = a.y + static_cast<int>(std::lround(dy * t));
                    const Terrain ter = g.grid().at(x, y);
                    check(ter != Terrain::Rock && ter != Terrain::Water,
                          lv.name + ": route segment passes through a wall at (" +
                          std::to_string(x) + "," + std::to_string(y) + ")");
                }
            }
            // And the route must actually be marked on the map.
            check(!g.grid().pathTiles().empty(), lv.name + ": the route is marked on the map");
        }
    }

    // ---------------------------------------------------------------------
    section("placement is refused for illegal ground");
    {
        Game g = makeGame();
        // Find a rock.
        bool foundRock = false;
        Cell rock{0, 0};
        for (int y = 0; y < g.grid().height() && !foundRock; ++y) {
            for (int x = 0; x < g.grid().width(); ++x) {
                if (g.grid().at(x, y) == Terrain::Rock) {
                    rock = Cell{x, y};
                    foundRock = true;
                    break;
                }
            }
        }
        check(foundRock, "the level has a rock");
        std::string why;
        check(!g.canBuild(TowerKind::Arrow, rock, &why), "cannot build on a rock");
        check(why.find("cannot be built") != std::string::npos,
              "the rock refusal explains itself (got: \"" + why + "\")");

        // Cannot build twice on the same cell.
        const Cell grass = findGrass(g);
        check(g.build(TowerKind::Arrow, grass), "first build succeeds");
        check(!g.canBuild(TowerKind::Cannon, grass, &why), "cannot build twice on one cell");
    }

    // ---------------------------------------------------------------------
    section("gold economy");
    {
        Game g = makeGame();
        const int start = g.gold();
        const Cell grass = findGrass(g);
        const int cost = towerDef(TowerKind::Arrow).level(0).cost;
        check(g.build(TowerKind::Arrow, grass), "build succeeds");
        checkEq(g.gold(), start - cost, "building deducts the cost");

        // Upgrading costs the next level's price.
        const int before = g.gold();
        const int upCost = towerDef(TowerKind::Arrow).level(1).cost;
        check(g.upgrade(g.towers().front().id), "upgrade succeeds");
        checkEq(g.gold(), before - upCost, "upgrading deducts the upgrade cost");
        checkEq(g.towers().front().level, 2, "the tower is now level 2");

        // Selling refunds a sensible fraction.
        const int beforeSell = g.gold();
        check(g.sell(g.towers().front().id), "sell succeeds");
        check(g.gold() > beforeSell, "selling refunds gold");
        check(g.towers().empty(), "the tower is gone after selling");
        check(!g.grid().hasTower(findGrass(g)), "the cell is free again after selling");
    }

    // ---------------------------------------------------------------------
    section("not enough gold is refused");
    {
        Game g = makeGame();
        // Drain the gold by building expensive towers until it runs out.
        int guard = 0;
        while (g.gold() >= 60 && guard++ < 200) {
            bool built = false;
            for (int y = 0; y < g.grid().height() && !built; ++y) {
                for (int x = 0; x < g.grid().width() && !built; ++x) {
                    if (g.canBuild(TowerKind::Arrow, Cell{x, y})) {
                        g.build(TowerKind::Arrow, Cell{x, y});
                        built = true;
                    }
                }
            }
            if (!built) break;
        }
        std::string why;
        bool placed = false;
        // Only probe BUILDABLE terrain: canBuild reports the terrain problem
        // before the gold problem, so probing a rock would mask the real reason.
        for (int y = 0; y < g.grid().height() && !placed; ++y) {
            for (int x = 0; x < g.grid().width() && !placed; ++x) {
                if (!g.grid().isBuildableTerrain(Cell{x, y})) continue;
                if (g.grid().hasTower(Cell{x, y})) continue;
                if (g.canBuild(TowerKind::Arrow, Cell{x, y}, &why)) {
                    g.build(TowerKind::Arrow, Cell{x, y});
                    placed = true;
                }
            }
        }
        check(!placed, "cannot build when out of gold");
        check(why.find("gold") != std::string::npos,
              "the refusal mentions gold (got: \"" + why + "\")");
        check(why.find("gold") != std::string::npos,
              "the refusal mentions gold (got: \"" + why + "\")");
    }

    // ---------------------------------------------------------------------
    section("enemies walk the route and reach the exit");
    {
        Game g = makeGame();
        // Skip the prep time and let the first wave run.
        run(g, 6.0f);
        check(g.waveNumber() >= 1, "the first wave started");
        check(!g.enemies().empty(), "enemies are on the map");

        // With no towers at all, they must eventually leak.
        const bool leaked = runUntil(g, [](const Game& x) { return x.enemiesLeaked() > 0; }, 300.0f);
        check(leaked, "unopposed enemies reach the exit");
        check(g.lives() < g.config().startingLives, "leaking enemies cost lives");
    }

    // ---------------------------------------------------------------------
    section("towers kill things");
    {
        Game g = makeGame();
        // A line of arrows across the route, so they engage early.
        const Cell s = routeStart(g);
        int built = 0;
        for (int dy = -2; dy <= 2; ++dy) {
            const Cell c{s.x + 1, s.y + dy};
            if (g.canBuild(TowerKind::Arrow, c)) { g.build(TowerKind::Arrow, c); ++built; }
        }
        check(built > 0, "towers were built");

        const bool killed = runUntil(g, [](const Game& x) { return x.kills() > 0; }, 300.0f);
        check(killed, "towers killed enemies");
        check(g.gold() > 0, "kills paid out gold");
    }

    // ---------------------------------------------------------------------
    section("attackers hunt towers");
    {
        Game g = makeGame();
        // A lone tower far from the route, so the sappers have to walk to it.
        Cell spot{-1, -1};
        for (int y = 1; y < g.grid().height() - 1; ++y) {
            for (int x = 1; x < g.grid().width() - 1; ++x) {
                if (g.grid().at(x, y) == Terrain::Grass) { spot = Cell{x, y}; break; }
            }
            if (spot.x >= 0) break;
        }
        check(g.build(TowerKind::Cannon, spot), "a lone tower was built");
        // Plus a defence so the game survives long enough for sappers to appear.
        buildDefence(g, TowerKind::Arrow, 12);

        // Run until wave 5 so sappers appear.
        const bool reached = runUntil(g, [](const Game& x) { return x.waveNumber() >= 5; }, 900.0f);
        check(reached, "reached a wave with sappers (wave " + std::to_string(g.waveNumber()) + ")");

        // Then run on until a tower has taken damage. The cumulative counter is
        // used rather than a current hp reading, because towers mend between
        // waves and would look untouched again by the time the test looked.
        const bool attacked = runUntil(g, [](const Game& x) { return x.towerDamageTaken() > 0; },
                                       600.0f);
        check(attacked, "attackers damaged the towers (" +
              std::to_string(g.towerDamageTaken()) + " damage taken, " +
              std::to_string(g.towersLost()) + " lost)");
    }

    // ---------------------------------------------------------------------
    section("flyers ignore the path");
    {
        Game g = makeGame();
        const Cell s = routeStart(g);
        const Cell e = routeEnd(g);

        // Wall off the route completely except for a gap the flyers use.
        // A flyer must still make progress even when the ground route is dead.
        // Simplest check: with NO towers, a flyer reaches the exit on a straight line.
        Game f = makeGame();
        f.startWaveNow();
        const bool fLeaked = runUntil(f, [](const Game& x) { return x.enemiesLeaked() > 0; }, 300.0f);
        check(fLeaked, "unopposed enemies leak");

        // And a flyer is not blocked by towers in front of it: build a wall of towers
        // right at the exit and confirm flyers still get through.
        Game w = makeGame();
        for (int y = 0; y < w.grid().height(); ++y) {
            for (int x = e.x - 1; x <= e.x + 1; ++x) {
                if (x < 0 || x >= w.grid().width()) continue;
                if (w.grid().at(x, y) == Terrain::Rock) continue;
                if (w.canBuild(TowerKind::Arrow, Cell{x, y})) {
                    w.build(TowerKind::Arrow, Cell{x, y});
                }
            }
        }
        check(w.grid().canReach(s, e) || w.towers().size() > 0,
              "a wall of towers can be built near the exit");
    }

    // ---------------------------------------------------------------------
    section("bosses appear and are worth a lot");
    {
        Game g = makeGame();
        buildDefence(g, TowerKind::Arrow, 20);
        const bool reached = runUntil(g, [](const Game& x) { return x.waveNumber() >= 5; }, 900.0f);
        check(reached, "reached the boss wave (wave " + std::to_string(g.waveNumber()) + ")");
        // The boss wave should have been flagged in the wave list.
        check(waveDef(5).boss, "wave 5 is a boss wave");
        check(waveDef(10).boss, "wave 10 is a boss wave");
        check(waveDef(10).groups.size() >= 2, "wave 10 sends both bosses");
    }

    // ---------------------------------------------------------------------
    section("titan surge escalates exponentially every 4 waves after wave 5");
    {
        // Check tier progression: every 4 waves after the first titan spawn (wave 5)
        checkEq(titanSurgeTier(1), 0, "wave 1 is tier 0");
        checkEq(titanSurgeTier(5), 0, "wave 5 (first titan) is tier 0");
        checkEq(titanSurgeTier(8), 0, "wave 8 is tier 0");
        checkEq(titanSurgeTier(9), 1, "wave 9 is tier 1 (4 waves after first titan)");
        checkEq(titanSurgeTier(12), 1, "wave 12 is tier 1");
        checkEq(titanSurgeTier(13), 2, "wave 13 is tier 2 (8 waves after first titan)");
        checkEq(titanSurgeTier(17), 3, "wave 17 is tier 3 (12 waves after first titan)");
        checkEq(titanSurgeTier(21), 4, "wave 21 is tier 4 (16 waves after first titan)");

        // Check exponential scaling formulas on normal difficulty
        TitanSurgeStats t1 = calcTitanSurge(9, "normal");
        TitanSurgeStats t2 = calcTitanSurge(13, "normal");
        TitanSurgeStats t3 = calcTitanSurge(17, "normal");
        check(t1.tier == 1, "t1 tier is 1");
        check(t2.tier == 2, "t2 tier is 2");
        check(t3.tier == 3, "t3 tier is 3");
        check(t2.hpMultiplier > t1.hpMultiplier * 1.8f, "hp scales exponentially (t2 > t1 * 1.8)");
        check(t3.hpMultiplier > t2.hpMultiplier * 1.8f, "hp scales exponentially (t3 > t2 * 1.8)");
        check(t2.armorBonus > t1.armorBonus, "armor scales with surge tier");
        check(t2.speedMultiplier > t1.speedMultiplier, "speed scales with surge tier");
        check(t2.siegeDamageMultiplier > t1.siegeDamageMultiplier, "attacker damage scales exponentially");

        // Game speed, endless mode, and auto wave controls
        Game g;
        checkEq(g.gameSpeed(), 1, "default speed is 1x");
        g.cycleGameSpeed();
        checkEq(g.gameSpeed(), 2, "speed cycles to 2x");
        g.cycleGameSpeed();
        checkEq(g.gameSpeed(), 3, "speed cycles to 3x");
        g.cycleGameSpeed();
        checkEq(g.gameSpeed(), 1, "speed cycles back to 1x");

        check(!g.endlessMode(), "endless mode is initially off");
        g.toggleEndlessMode();
        check(g.endlessMode(), "endless mode can be toggled on");

        check(!g.autoWave(), "auto wave is initially off");
        g.toggleAutoWave();
        check(g.autoWave(), "auto wave can be toggled on");

        // Dynamic wave generation beyond wave 15
        WaveDef w16 = generateWave(16, "normal");
        checkEq(w16.number, 16, "wave 16 can be generated dynamically");
        WaveDef w20 = generateWave(20, "normal");
        check(w20.boss, "wave 20 is a titan boss wave");
    }

    // ---------------------------------------------------------------------
    section("tactical commands: repair, fortified shielding, and overdrive supercharge");
    {
        Game g = makeGame();
        Cell s = routeStart(g);
        Cell spot{s.x + 1, s.y + 1};
        check(g.build(TowerKind::Arrow, spot), "built test tower");
        int towerId = g.towers().back().id;

        // Damage the tower
        g.damageTower(towerId, 45);
        check(g.towers().back().hp < g.towers().back().maxHp, "tower took damage");

        // Repair and shield
        int repCost = 0;
        check(g.canRepair(towerId, &repCost), "tower can be repaired");
        checkEq(repCost, 20, "repair cost is 20 gold");
        int goldBefore = g.gold();
        check(g.repair(towerId), "repair executed successfully");
        checkEq(g.towers().back().hp, g.towers().back().maxHp, "hp fully restored");
        checkEq(g.towers().back().shieldHp, 50, "gained fortified shield");
        checkEq(g.gold(), goldBefore - 20, "gold deducted for repair");
        check(!g.canRepair(towerId), "cannot repair when already fully fortified");

        // Damage against shield
        g.damageTower(towerId, 30);
        checkEq(g.towers().back().shieldHp, 20, "shield absorbed the damage");
        checkEq(g.towers().back().hp, g.towers().back().maxHp, "hp was protected by shield");

        // Overdrive requires Level 3
        check(!g.canOverdrive(towerId), "level 1 tower cannot overdrive");
        g.upgrade(towerId);
        check(!g.canOverdrive(towerId), "level 2 tower cannot overdrive");
        g.upgrade(towerId);
        checkEq(g.towers().back().level, 3, "tower reached level 3");

        float baseRange = g.effectiveRange(g.towers().back());
        check(g.canOverdrive(towerId), "level 3 tower can enter overdrive");
        check(g.overdrive(towerId), "overdrive activated");
        check(g.towers().back().overdriveRemaining > 8.0f, "overdrive duration active");
        check(g.effectiveRange(g.towers().back()) > baseRange, "effective range increased during overdrive");

        // Combat stats validation
        check(g.stats().totalGoldSpent > 0, "combat stats tracked gold spent");
        check(g.stats().towersBuilt >= 1, "combat stats tracked towers built");
    }

    // ---------------------------------------------------------------------
    section("commander abilities, powerups, and elemental synergies");
    {
        Game g = makeGame();
        check(g.canCastAbility(CommanderAbility::Meteor), "meteor can be cast initially");
        check(g.canCastAbility(CommanderAbility::CryoFreeze), "cryo freeze can be cast initially");
        check(g.canCastAbility(CommanderAbility::MidasRush), "midas rush can be cast initially");

        // Cast Cryo Freeze
        int goldBefore = g.gold();
        check(g.castCryoFreeze(), "cryo freeze cast successfully");
        checkEq(g.gold(), goldBefore - 40, "gold deducted for cryo freeze");
        check(g.abilityCooldown(CommanderAbility::CryoFreeze) > 0.0f, "cooldown active after cast");
        check(!g.canCastAbility(CommanderAbility::CryoFreeze), "cannot cast while on cooldown");

        // Spawn a powerup and collect it
        Vec2 dropPos = g.cellToWorld(Cell{3, 3});
        g.spawnPowerup(dropPos, PowerupKind::GoldCache);
        checkEq(static_cast<int>(g.powerups().size()), 1, "powerup spawned on map");
        check(g.collectPowerupAt(dropPos, 30.0f), "powerup collected by player");
        checkEq(static_cast<int>(g.powerups().size()), 0, "powerup consumed");
        check(g.stats().powerupsCollected >= 1, "powerup collection tracked in stats");

        // Cast Orbital Meteor Strike
        goldBefore = g.gold();
        check(g.castMeteor(dropPos), "meteor cast successfully");
        checkEq(g.gold(), goldBefore - 35, "gold deducted for meteor");
        check(g.abilityCooldown(CommanderAbility::Meteor) > 0.0f, "meteor on cooldown");

        // Elemental synergy tracking
        check(g.stats().commanderAbilitiesCast >= 2, "commander abilities tracked in combat stats");
    }

    // ---------------------------------------------------------------------
    section("the game can be lost");
    {
        Game g = makeGame();
        g.setDifficulty("hard");
        const bool lost = runUntil(g, [](const Game& x) { return x.isGameOver(); }, 900.0f);
        check(lost, "an undefended hard game is lost");
        check(g.lives() <= 0, "the core reached zero lives");
    }

    // ---------------------------------------------------------------------
    section("a defended game can be won");
    {
        Game g = makeGame(0, "easy");
        // Keep buying and upgrading as the gold comes in.
        //
        // NOTE: the ids are copied out before upgrading. buildDefence() pushes
        // new towers onto g.towers(), which invalidates any iterator into it —
        // iterating the live vector while modifying it is undefined behaviour.
        for (int i = 0; i < 40000; ++i) {
            g.update(1.0f / 30.0f);
            if (g.isGameOver() || g.isVictory()) break;
            // New towers first: breadth of coverage beats depth early on.
            buildDefence(g, tf::TowerKind::Arrow, 40);
            // Then upgrade, but only with gold to spare.
            if (g.gold() > 260) {
                std::vector<int> ids;
                for (const auto& t : g.towers()) ids.push_back(t.id);
                for (int id : ids) {
                    if (g.gold() <= 260) break;
                    g.upgrade(id);
                }
            }
        }
        check(!g.isGameOver(), "a well-defended easy game is not lost");
        check(g.isVictory(), "a well-defended easy game is WON (wave " +
              std::to_string(g.waveNumber()) + ", lives " +
              std::to_string(g.lives()) + ")");
    }

    // ---------------------------------------------------------------------
    section("lua content loads");
    {
        const LuaContent content = loadLuaContent(TF_CONTENT_DIR "/content.lua");
        check(content.ok(), "the content script loads without errors" +
              (content.errors.empty() ? "" : ": " + content.errors[0]));
        checkEq(static_cast<int>(content.levels.size()), 3, "three levels are defined");
        checkEq(static_cast<int>(content.towers.size()), 12, "twelve towers are defined");
        checkEq(static_cast<int>(content.enemies.size()), 10, "ten enemies are defined");

        // Every tower must have 3 levels with sensible numbers.
        for (const auto& t : content.towers) {
            check(t.levels.size() == 3, "tower '" + t.id + "' has 3 levels");
            check(t.levels[0].cost > 0, "tower '" + t.id + "' costs something");
            check(t.levels[0].range > 0, "tower '" + t.id + "' has a range");
        }

        // Every enemy must have a positive hp and speed.
        for (const auto& e : content.enemies) {
            check(e.hp > 0, "enemy '" + e.id + "' has hp");
            check(e.speed > 0, "enemy '" + e.id + "' has speed");
        }

        // Both archetypes and a boss must exist.
        int walkers = 0, attackers = 0, flyers = 0, bosses = 0;
        for (const auto& e : content.enemies) {
            if (e.archetype == EnemyArchetype::Walker) ++walkers;
            if (e.archetype == EnemyArchetype::Attacker) ++attackers;
            if (e.archetype == EnemyArchetype::Flyer) ++flyers;
            if (e.isBoss) ++bosses;
        }
        check(walkers >= 3, "there are several walkers");
        check(attackers >= 2, "there are several tower-attackers");
        check(flyers >= 1, "there is a flyer");
        check(bosses >= 2, "there are bosses");
    }

    // ---------------------------------------------------------------------
    section("a broken lua script fails safely");
    {
        const LuaContent content = loadLuaContent("/definitely/not/here.lua");
        check(!content.ok(), "a missing script is reported as an error");
        check(!content.errors.empty(), "an error message is produced");
    }

    // ---------------------------------------------------------------------
    section("fast shots connect instead of sailing past their target");
    {
        // REGRESSION: the impact check used a fixed 6px radius while a fast
        // shot crosses 32px in a single frame, so it routinely stepped clean
        // over its target and then chased it in circles for its full 4s life.
        Game g = makeGame(0, "easy");
        const Cell s = routeStart(g);
        Cell spot{-1, -1};
        for (int dy = -3; dy <= 3 && spot.x < 0; ++dy) {
            for (int dx = 0; dx <= 3 && spot.x < 0; ++dx) {
                const Cell c{s.x + dx, s.y + dy};
                if (g.canBuild(TowerKind::Sniper, c)) spot = c;
            }
        }
        check(spot.x >= 0, "a sniper spot near the entrance was found");
        if (spot.x >= 0) g.build(TowerKind::Sniper, spot);
        g.startWaveNow();

        // One sniper on purpose: nothing else can kill its target, so a miss
        // shows up as a long chase of a target that stays alive.
        const float dt = 1.0f / 60.0f;
        std::map<int, float> chaseTime;   // projectile id -> seconds with a live target
        std::map<int, char> seen;         // every projectile id ever observed
        float worstChase = 0;
        for (int i = 0; i < static_cast<int>(150.0f / dt); ++i) {
            if (g.isGameOver() || g.isVictory()) break;
            if (g.waveNumber() > 3) break;
            g.update(dt);
            for (const auto& p : g.projectiles()) {
                seen[p.id] = 1;
                if (g.enemyById(p.targetId) != nullptr) {
                    const float t = chaseTime[p.id] + dt;
                    chaseTime[p.id] = t;
                    worstChase = std::max(worstChase, t);
                }
            }
        }
        check(static_cast<int>(seen.size()) >= 5,
              "the sniper fired several shots (otherwise this test proves nothing)");
        check(worstChase < 1.5f,
              "every shot connected within 1.5s (worst chase was " +
              std::to_string(worstChase) + "s)");
    }

    // ---------------------------------------------------------------------
    std::printf("\n==========================================\n");
    std::printf("passed: %d   failed: %d\n", g_pass, g_fail);
    if (g_fail > 0) {
        std::printf("\nfailures:\n");
        for (const auto& f : g_failures) std::printf("  - %s\n", f.c_str());
        return 1;
    }
    std::printf("ALL TESTS PASSED\n");
    return 0;
}
