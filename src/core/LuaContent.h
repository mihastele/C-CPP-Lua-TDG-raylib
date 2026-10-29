// towerfall — Lua content loading.
//
// The C++ core owns the simulation; Lua owns the CONTENT. Levels, wave
// definitions and tower balance tables are written in Lua, so a designer can
// change the game without touching C++ or recompiling.
//
// A Lua script returns a table:
//     return {
//       name = "Meadow Run",
//       difficulty = "easy",
//       rows = { "########", "#......#", ... },
//       waves = { { name = "First Contact", groups = { {kind="grunt", count=6} } } },
//     }

#pragma once

#include <string>
#include <vector>

#include "core/Defs.h"
#include "core/Types.h"
#include "core/Waves.h"

namespace tf {

struct LuaTowerDef {
    std::string id;
    TowerKind kind = TowerKind::Arrow;
    std::string name;
    std::string description;
    std::vector<TowerLevel> levels;
    float projectileSpeed = 12.0f;
    AttackMode mode = AttackMode::Single;
    ProjectileStyle style = ProjectileStyle::Arrow;
    bool targetsAir = true;
    bool targetsGround = true;
    std::string upgradeNote;
};

struct LuaEnemyDef {
    std::string id;
    EnemyKind kind = EnemyKind::Grunt;
    std::string name;
    EnemyArchetype archetype = EnemyArchetype::Walker;
    int hp = 10;
    float speed = 1.0f;
    int damage = 0;
    float attackRange = 0.9f;
    float attackRate = 0.5f;
    int bounty = 1;
    int armor = 0;
    float scale = 1.0f;
    bool slowResistant = false;
    bool isBoss = false;
    std::string color = "red";
    std::string shape = "blob";
};

// The result of loading the content script.
struct LuaContent {
    std::vector<LevelDef> levels;
    std::vector<LuaTowerDef> towers;
    std::vector<LuaEnemyDef> enemies;
    std::vector<std::string> errors;
    bool ok() const { return errors.empty(); }
};

/**
 * Loads a content script.
 *
 * Never throws and never aborts: problems are collected into `errors` so the
 * game can show them on screen and carry on with whatever did load. A missing
 * file is an error, not a crash.
 */
LuaContent loadLuaContent(const std::string& path);

// Resolves a tower kind from its script id ("arrow", "cannon", ...).
bool towerKindFromId(const std::string& id, TowerKind* out);
bool enemyKindFromId(const std::string& id, EnemyKind* out);

const char* towerKindId(TowerKind k);
const char* enemyKindId(EnemyKind k);

}  // namespace tf
