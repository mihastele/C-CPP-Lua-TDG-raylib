// towerfall — tower and enemy definitions.
//
// All the balance numbers live here rather than being scattered through the
// simulation, so tuning a tower is a one-line change and the content is easy to
// review as a table.

#pragma once

#include <string>
#include <vector>

#include "core/Types.h"

namespace tf {

// ------------------------------------------------------------------ towers

enum class AttackMode : uint8_t {
    Single = 0,    // one target
    Splash = 1,    // area around the target
    Chain = 2,     // bounces to nearby enemies
    Pierce = 3,    // hits everything in a line
    Beam = 4,      // instant hitscan, pierces
    Aura = 5,      // continuous damage in a radius
};

enum class ProjectileStyle : uint8_t {
    Arrow = 0,
    Cannonball = 1,
    FrostShard = 2,
    Lightning = 3,
    Bullet = 4,
    PoisonBottle = 5,
    Fireball = 6,
    Prism = 7,
    MortarShell = 8,
    Pierce = 9,
    Aura = 10,
};

struct TowerLevel {
    int cost = 0;               // cost to reach this level (level 1 = build cost)
    int damage = 0;
    float range = 0;            // in tiles
    float fireRate = 0;         // shots per second
    float splashRadius = 0;     // tiles
    int chainTargets = 0;
    float slowFactor = 0;       // 0.4 == 40% slower
    float slowDuration = 0;
    float statusDuration = 0;   // burn/poison duration
    float statusDps = 0;
    int pierceCount = 0;
    float auraDps = 0;
};

struct TowerDef {
    TowerKind kind = TowerKind::Arrow;
    std::string name;
    std::string description;
    AttackMode mode = AttackMode::Single;
    ProjectileStyle style = ProjectileStyle::Arrow;
    std::vector<TowerLevel> levels;    // 3 entries: levels 1, 2, 3
    float projectileSpeed = 12.0f;
    float turnRate = 8.0f;             // radians per second
    bool targetsAir = true;
    bool targetsGround = true;
    bool canTargetAttackers = true;    // can it shoot enemy units that attack towers?
    std::string upgradeNote;           // shown in the shop when this level is next

    const TowerLevel& level(int i) const {
        static const TowerLevel fallback;
        if (levels.empty()) return fallback;
        const int idx = i < 0 ? 0 : (i >= static_cast<int>(levels.size())
                                          ? static_cast<int>(levels.size()) - 1 : i);
        return levels[static_cast<size_t>(idx)];
    }
    int maxLevel() const { return static_cast<int>(levels.size()); }
};

// ------------------------------------------------------------------ enemies

enum class EnemyArchetype : uint8_t {
    Walker = 0,     // follows the path to the exit
    Attacker = 1,   // walks toward towers and damages them
    Flyer = 2,      // flies straight to the exit, ignores the path
};

struct EnemyDef {
    EnemyKind kind = EnemyKind::Grunt;
    std::string name;
    EnemyArchetype archetype = EnemyArchetype::Walker;
    int hp = 10;
    float speed = 2.0f;            // tiles per second
    int damage = 0;                // damage to a tower (attackers only)
    float attackRange = 0;         // tiles
    float attackRate = 0;          // attacks per second
    int bounty = 1;                // gold on kill
    int armor = 0;                 // PERCENT damage reduction (0-75)
    float scale = 1.0f;            // sprite scale
    bool slowResistant = false;
    bool isBoss = false;
    std::string color = "red";     // sprite palette hint
    std::string shape = "blob";
};

// ------------------------------------------------------------------ catalogue

const std::vector<TowerDef>& allTowerDefs();
const std::vector<EnemyDef>& allEnemyDefs();

const TowerDef& towerDef(TowerKind kind);
const EnemyDef& enemyDef(EnemyKind kind);

// Number of distinct upgrade levels available (1..3).
int towerUpgradeCount(TowerKind kind);

}  // namespace tf
