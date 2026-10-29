// towerfall — core types shared by the simulation and the presentation layer.
//
// Nothing here depends on raylib: the simulation must be testable headlessly.

#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace tf {

// Forward declaration: Grid.h includes Types.h, so Grid cannot be named here.
class Grid;

}  // namespace tf

namespace tf {

// ------------------------------------------------------------------ geometry

struct Vec2 {
    float x = 0, y = 0;

    Vec2() = default;
    Vec2(float xx, float yy) : x(xx), y(yy) {}

    Vec2 operator+(const Vec2& o) const { return {x + o.x, y + o.y}; }
    Vec2 operator-(const Vec2& o) const { return {x - o.x, y - o.y}; }
    Vec2 operator*(float s) const { return {x * s, y * s}; }
    Vec2& operator+=(const Vec2& o) { x += o.x; y += o.y; return *this; }
    Vec2& operator-=(const Vec2& o) { x -= o.x; y -= o.y; return *this; }

    float length() const;
    float lengthSquared() const;
    Vec2 normalized() const;
    float distanceTo(const Vec2& o) const;
};

struct Cell {
    int x = 0, y = 0;

    Cell() = default;
    Cell(int xx, int yy) : x(xx), y(yy) {}

    bool operator==(const Cell& o) const { return x == o.x && y == o.y; }
    bool operator!=(const Cell& o) const { return !(*this == o); }
    bool operator<(const Cell& o) const {
        return y != o.y ? y < o.y : x < o.x;
    }

    int index() const { return y * 100000 + x; }   // only used for hashing
};

// ------------------------------------------------------------------ enums

enum class Terrain : uint8_t {
    Grass = 0,   // buildable
    Path = 1,    // the intended route — buildable, but enemies prefer it
    Rock = 2,    // not buildable, blocks movement
    Water = 3,   // not buildable, blocks movement
    Sand = 4,    // buildable
};

enum class TowerKind : uint8_t {
    Arrow = 0,     // fast, single target, cheap
    Cannon = 1,    // slow, splash damage
    Frost = 2,     // slows
    Tesla = 3,     // chains between enemies
    Sniper = 4,    // very long range, very slow, high damage
    Poison = 5,    // damage over time
    Flame = 6,     // short range, fast, burn
    Prism = 7,     // splits into piercing beams
    Mortar = 8,    // long-range arcing shell, splash
    Ballista = 9,  // huge single-target damage
    Tarpit = 10,   // aura: slows and damages over time
    Beacon = 11,   // aura: buffs nearby towers
};

enum class EnemyKind : uint8_t {
    Grunt = 0,      // baseline walker
    Runner = 1,     // fast, low hp
    Brute = 2,      // slow, high hp
    Flyer = 3,      // ignores the path, flies straight
    Crawler = 4,    // very slow, very tough, armored
    Sapper = 5,     // attacks towers
    Bruiser = 6,    // attacks towers, tanky
    Wraith = 7,     // walks, drains hearts / ignores slow
    Boss = 8,       // boss walker
    BossSiege = 9,  // boss that attacks towers
};

enum class StatusEffect : uint8_t {
    None = 0,
    Slow = 1,
    Burn = 2,
    Poison = 3,
    Stun = 4,
};

inline bool isBoss(EnemyKind k) {
    return k == EnemyKind::Boss || k == EnemyKind::BossSiege;
}

// Enemies that attack towers rather than walking the path.
inline bool isAttacker(EnemyKind k) {
    return k == EnemyKind::Sapper
        || k == EnemyKind::Bruiser
        || k == EnemyKind::BossSiege;
}

// Enemies that ignore the path entirely and fly straight to the exit.
inline bool isFlying(EnemyKind k) {
    return k == EnemyKind::Flyer;
}

// ------------------------------------------------------------------ config

struct GameConfig {
    int gridW = 24;
    int gridH = 16;
    int tileSize = 48;

    int startingLives = 20;
    int startingGold = 260;

    float waveDelay = 12.0f;       // seconds between waves
    float prepTime = 5.0f;         // seconds before the first wave starts

    bool allowFlyersToIgnoreTowers = true;
    bool bossesIgnoreSlow = true;
};

// One row of the level file.
struct LevelDef {
    std::string name;
    std::string difficulty;
    int width = 0;
    int height = 0;
    std::vector<std::string> rows;     // ASCII art of the terrain
    int startX = 0, startY = 0;        // where enemies enter
    int endX = 0, endY = 0;            // where they want to reach
    std::string background = "field";

    // The intended route, as a list of waypoints. The tiles along it are marked
    // as Path so the player can SEE it, and so enemies prefer to walk it.
    //
    // The route is a hint, not a rule: enemies re-path around towers, and the
    // player may wall the route off (just not completely).
    std::vector<Cell> waypoints;

    /** Marks the waypoint route on the grid as Path terrain. */
    void applyRoute(Grid* grid) const;
};

}  // namespace tf
