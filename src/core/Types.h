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

enum class TargetPriority : uint8_t {
    First = 0,     // closest to the exit (default)
    Strongest = 1, // highest maximum & current HP (boss killer)
    Weakest = 2,   // lowest current HP (finishers)
    Titans = 3,    // prioritizes bosses/titans over standard mobs
};

enum class PowerupKind : uint8_t {
    None = 0,
    GoldCache,       // +30 bonus gold
    OverchargeAll,   // +6s overdrive for all towers on map
    CryoFreeze,      // freezes all creeps for 3s
    CoreLife,        // +1 Core Life
};

inline const char* powerupName(PowerupKind k) {
    switch (k) {
        case PowerupKind::GoldCache:     return "Gold Cache (+30g)";
        case PowerupKind::OverchargeAll: return "Overcharge All Towers";
        case PowerupKind::CryoFreeze:    return "Cryo Blizzard";
        case PowerupKind::CoreLife:      return "Core Repair (+1 Life)";
        default: return "";
    }
}

enum class CommanderAbility : uint8_t {
    Meteor = 0,     // Orbital meteor bombardment
    CryoFreeze,     // Flash freeze blizzard
    MidasRush,      // Transmute weak creeps into gold
    Count
};

struct CombatStats {
    uint64_t totalDamageDealt = 0;
    int titansDefeated = 0;
    int maxSurgeTier = 0;
    int totalEnemiesKilled = 0;
    int totalGoldEarned = 0;
    int totalGoldSpent = 0;
    int towersBuilt = 0;
    int towersLost = 0;
    int commanderAbilitiesCast = 0;
    int powerupsCollected = 0;
    int combosTriggered = 0;
    uint64_t damageByTowerKind[12] = {0};
};

inline const char* targetPriorityName(TargetPriority p) {
    switch (p) {
        case TargetPriority::First:     return "First";
        case TargetPriority::Strongest: return "Strongest";
        case TargetPriority::Weakest:   return "Weakest";
        case TargetPriority::Titans:    return "Titans";
        default: return "First";
    }
}

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

enum class EnemyAffix : uint8_t {
    None = 0,
    Frenzied = 1,     // +35% move speed, +25% attack rate
    Juggernaut = 2,   // +80% max HP, +4 armor, immune to slow
    Regenerator = 3,  // heals 4% max HP per second
    Vengeful = 4,     // explodes on death, damages towers in radius & speeds up adjacent enemies
    Shielded = 5,     // absorbs incoming damage with energy barrier
};

inline const char* affixName(EnemyAffix a) {
    switch (a) {
        case EnemyAffix::Frenzied:    return "Frenzied";
        case EnemyAffix::Juggernaut:  return "Juggernaut";
        case EnemyAffix::Regenerator: return "Regenerating";
        case EnemyAffix::Vengeful:    return "Vengeful";
        case EnemyAffix::Shielded:    return "Shielded";
        default: return "";
    }
}

// ---- Titan Surge Difficulty Calculations ----
// First titan spawn is wave 5 (Colossus).
// Every 4 waves after wave 5 (waves 9, 13, 17, 21...), a new Titan Surge Tier begins,
// exponentially escalating HP, speed, armor, attacker siege damage, and elite affixes.
inline int titanSurgeTier(int wave) {
    if (wave < 5) return 0;
    return (wave - 5) / 4;
}

inline const char* titanSurgeName(int tier) {
    switch (tier) {
        case 0: return "";
        case 1: return "TITAN SURGE I";
        case 2: return "TITAN CATACLYSM II";
        case 3: return "TITAN APOCALYPSE III";
        case 4: return "TITAN EXTINCTION IV";
        default: return "TITAN ANNIHILATION";
    }
}

struct TitanSurgeStats {
    int tier = 0;
    float hpMultiplier = 1.0f;
    float speedMultiplier = 1.0f;
    int armorBonus = 0;
    float siegeDamageMultiplier = 1.0f;
    float affixChance = 0.0f;
};

inline TitanSurgeStats calcTitanSurge(int wave, const std::string& difficulty) {
    TitanSurgeStats s;
    if (difficulty == "easy") return s; // Easy mode maintains standard intro curve for tests & learners

    s.tier = titanSurgeTier(wave);
    if (s.tier <= 0) return s;

    // Exponential scaling base factor per tier (every 4 waves after Wave 5)
    float baseGrowth = 1.85f;
    if (difficulty == "nightmare") baseGrowth = 2.45f;
    else if (difficulty == "hard") baseGrowth = 2.15f;

    float tierF = static_cast<float>(s.tier);
    // Exponential formula: (baseGrowth)^tier
    float hpExp = 1.0f;
    for (int i = 0; i < s.tier; ++i) hpExp *= baseGrowth;
    s.hpMultiplier = hpExp;

    s.speedMultiplier = 1.0f + 0.12f * tierF;
    s.armorBonus = 2 * s.tier;

    float siegeExp = 1.0f;
    for (int i = 0; i < s.tier; ++i) siegeExp *= 1.35f;
    s.siegeDamageMultiplier = siegeExp;

    s.affixChance = std::min(0.85f, 0.20f + 0.15f * tierF);
    return s;
}

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
