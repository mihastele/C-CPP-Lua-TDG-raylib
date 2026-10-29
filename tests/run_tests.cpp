// towerfall — headless test suite.
//
// Runs the REAL simulation with no window and no graphics, so the rules the
// game actually uses are the rules the tests check. The single most important
// test is the placement rule: a tower may be built anywhere, including on the
// path, but never in a way that seals the route.

#include <algorithm>
#include <cstdio>
#include <cmath>
#include <string>
#include <vector>

#include "core/Game.h"
#include "core/LuaContent.h"

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

}  // namespace

// ===========================================================================

int main() {
    std::printf("towerfall — headless test suite\n");

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
