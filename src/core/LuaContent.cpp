#include "core/LuaContent.h"

#include <cstring>

extern "C" {
#include "lauxlib.h"
#include "lua.h"
#include "lualib.h"
}

#include <map>

namespace tf {
namespace {

// ---- id <-> enum mappings -------------------------------------------------

const std::map<std::string, TowerKind>& towerKindMap() {
    static const std::map<std::string, TowerKind> m = {
        {"arrow", TowerKind::Arrow},       {"cannon", TowerKind::Cannon},
        {"frost", TowerKind::Frost},       {"tesla", TowerKind::Tesla},
        {"sniper", TowerKind::Sniper},     {"poison", TowerKind::Poison},
        {"flame", TowerKind::Flame},       {"prism", TowerKind::Prism},
        {"mortar", TowerKind::Mortar},     {"ballista", TowerKind::Ballista},
        {"tarpit", TowerKind::Tarpit},     {"beacon", TowerKind::Beacon},
    };
    return m;
}

const std::map<std::string, EnemyKind>& enemyKindMap() {
    static const std::map<std::string, EnemyKind> m = {
        {"grunt", EnemyKind::Grunt},       {"runner", EnemyKind::Runner},
        {"brute", EnemyKind::Brute},       {"flyer", EnemyKind::Flyer},
        {"crawler", EnemyKind::Crawler},   {"wraith", EnemyKind::Wraith},
        {"sapper", EnemyKind::Sapper},     {"bruiser", EnemyKind::Bruiser},
        {"boss", EnemyKind::Boss},         {"boss_siege", EnemyKind::BossSiege},
    };
    return m;
}

// ---- small Lua reading helpers -------------------------------------------
//
// Each of these tolerates a missing field by returning the supplied default,
// so a content script only has to specify what it wants to change.

std::string readString(lua_State* L, const char* key, const std::string& def) {
    lua_getfield(L, -1, key);
    std::string out = def;
    if (lua_isstring(L, -1)) out = lua_tostring(L, -1);
    lua_pop(L, 1);
    return out;
}

int readInt(lua_State* L, const char* key, int def) {
    lua_getfield(L, -1, key);
    int out = def;
    if (lua_isnumber(L, -1)) out = static_cast<int>(lua_tonumber(L, -1));
    lua_pop(L, 1);
    return out;
}

float readFloat(lua_State* L, const char* key, float def) {
    lua_getfield(L, -1, key);
    float out = def;
    if (lua_isnumber(L, -1)) out = static_cast<float>(lua_tonumber(L, -1));
    lua_pop(L, 1);
    return out;
}

bool readBool(lua_State* L, const char* key, bool def) {
    lua_getfield(L, -1, key);
    bool out = def;
    if (!lua_isnil(L, -1)) out = lua_toboolean(L, -1) != 0;
    lua_pop(L, 1);
    return out;
}

// Reads a table of strings (an array of row strings).
std::vector<std::string> readStringArray(lua_State* L, const char* key) {
    std::vector<std::string> out;
    lua_getfield(L, -1, key);
    if (lua_istable(L, -1)) {
        const lua_Integer n = luaL_len(L, -1);
        for (lua_Integer i = 1; i <= n; ++i) {
            lua_rawgeti(L, -1, i);
            if (lua_isstring(L, -1)) out.push_back(lua_tostring(L, -1));
            lua_pop(L, 1);
        }
    }
    lua_pop(L, 1);
    return out;
}

TowerLevel readLevel(lua_State* L) {
    TowerLevel l;
    l.cost = readInt(L, "cost", 0);
    l.damage = readInt(L, "damage", 0);
    l.range = readFloat(L, "range", 0);
    l.fireRate = readFloat(L, "fireRate", 0);
    l.splashRadius = readFloat(L, "splashRadius", 0);
    l.chainTargets = readInt(L, "chainTargets", 0);
    l.slowFactor = readFloat(L, "slowFactor", 0);
    l.slowDuration = readFloat(L, "slowDuration", 0);
    l.statusDuration = readFloat(L, "statusDuration", 0);
    l.statusDps = readFloat(L, "statusDps", 0);
    l.pierceCount = readInt(L, "pierceCount", 0);
    l.auraDps = readFloat(L, "auraDps", 0);
    return l;
}

}  // namespace

bool towerKindFromId(const std::string& id, TowerKind* out) {
    const auto& m = towerKindMap();
    auto it = m.find(id);
    if (it == m.end()) return false;
    if (out) *out = it->second;
    return true;
}

bool enemyKindFromId(const std::string& id, EnemyKind* out) {
    const auto& m = enemyKindMap();
    auto it = m.find(id);
    if (it == m.end()) return false;
    if (out) *out = it->second;
    return true;
}

const char* towerKindId(TowerKind k) {
    for (const auto& p : towerKindMap()) if (p.second == k) return p.first.c_str();
    return "arrow";
}

const char* enemyKindId(EnemyKind k) {
    for (const auto& p : enemyKindMap()) if (p.second == k) return p.first.c_str();
    return "grunt";
}

LuaContent loadLuaContent(const std::string& path) {
    LuaContent content;

    lua_State* L = luaL_newstate();
    if (!L) {
        content.errors.push_back("could not create a Lua state");
        return content;
    }
    luaL_openlibs(L);

    if (luaL_dofile(L, path.c_str()) != LUA_OK) {
        const char* msg = lua_tostring(L, -1);
        content.errors.push_back(std::string("Lua error in ") + path + ": " +
                                 (msg ? msg : "(unknown)"));
        lua_close(L);
        return content;
    }

    // The script must return a table.
    if (!lua_istable(L, -1)) {
        content.errors.push_back(path + ": the script must return a table");
        lua_close(L);
        return content;
    }

    // ---- levels
    lua_getfield(L, -1, "levels");
    if (lua_istable(L, -1)) {
        const lua_Integer n = luaL_len(L, -1);
        for (lua_Integer i = 1; i <= n; ++i) {
            lua_rawgeti(L, -1, i);
            if (!lua_istable(L, -1)) { lua_pop(L, 1); continue; }

            LevelDef lv;
            lv.name = readString(L, "name", "Level " + std::to_string(i));
            lv.difficulty = readString(L, "difficulty", "normal");
            lv.rows = readStringArray(L, "rows");

            if (lv.rows.empty()) {
                content.errors.push_back("level '" + lv.name + "' has no rows");
            } else {
                // All rows must be the same width, or the grid is ragged.
                const size_t w = lv.rows[0].size();
                for (const auto& r : lv.rows) {
                    if (r.size() != w) {
                        content.errors.push_back("level '" + lv.name + "' has ragged rows");
                        break;
                    }
                }
                lv.width = static_cast<int>(w);
                lv.height = static_cast<int>(lv.rows.size());
            }
            content.levels.push_back(lv);
            lua_pop(L, 1);
        }
    }
    lua_pop(L, 1);

    // ---- towers
    lua_getfield(L, -1, "towers");
    if (lua_istable(L, -1)) {
        const lua_Integer n = luaL_len(L, -1);
        for (lua_Integer i = 1; i <= n; ++i) {
            lua_rawgeti(L, -1, i);
            if (!lua_istable(L, -1)) { lua_pop(L, 1); continue; }

            LuaTowerDef t;
            t.id = readString(L, "id", "");
            if (!towerKindFromId(t.id, &t.kind)) {
                content.errors.push_back("unknown tower id '" + t.id + "'");
                lua_pop(L, 1);
                continue;
            }
            t.name = readString(L, "name", t.id);
            t.description = readString(L, "description", "");
            t.upgradeNote = readString(L, "upgradeNote", "");
            t.projectileSpeed = readFloat(L, "projectileSpeed", 12.0f);
            t.targetsAir = readBool(L, "targetsAir", true);
            t.targetsGround = readBool(L, "targetsGround", true);

            const std::string mode = readString(L, "mode", "single");
            if (mode == "splash") t.mode = AttackMode::Splash;
            else if (mode == "chain") t.mode = AttackMode::Chain;
            else if (mode == "pierce") t.mode = AttackMode::Pierce;
            else if (mode == "beam") t.mode = AttackMode::Beam;
            else if (mode == "aura") t.mode = AttackMode::Aura;
            else t.mode = AttackMode::Single;

            lua_getfield(L, -1, "levels");
            if (lua_istable(L, -1)) {
                const lua_Integer ln = luaL_len(L, -1);
                for (lua_Integer j = 1; j <= ln; ++j) {
                    lua_rawgeti(L, -1, j);
                    if (lua_istable(L, -1)) t.levels.push_back(readLevel(L));
                    lua_pop(L, 1);
                }
            }
            lua_pop(L, 1);

            if (t.levels.empty()) {
                content.errors.push_back("tower '" + t.id + "' has no levels");
            }
            content.towers.push_back(t);
            lua_pop(L, 1);
        }
    }
    lua_pop(L, 1);

    // ---- enemies
    lua_getfield(L, -1, "enemies");
    if (lua_istable(L, -1)) {
        const lua_Integer n = luaL_len(L, -1);
        for (lua_Integer i = 1; i <= n; ++i) {
            lua_rawgeti(L, -1, i);
            if (!lua_istable(L, -1)) { lua_pop(L, 1); continue; }

            LuaEnemyDef e;
            e.id = readString(L, "id", "");
            if (!enemyKindFromId(e.id, &e.kind)) {
                content.errors.push_back("unknown enemy id '" + e.id + "'");
                lua_pop(L, 1);
                continue;
            }
            e.name = readString(L, "name", e.id);
            const std::string arch = readString(L, "archetype", "walker");
            if (arch == "attacker") e.archetype = EnemyArchetype::Attacker;
            else if (arch == "flyer") e.archetype = EnemyArchetype::Flyer;
            else e.archetype = EnemyArchetype::Walker;

            e.hp = readInt(L, "hp", 10);
            e.speed = readFloat(L, "speed", 1.0f);
            e.damage = readInt(L, "damage", 0);
            e.attackRange = readFloat(L, "attackRange", 0.9f);
            e.attackRate = readFloat(L, "attackRate", 0.5f);
            e.bounty = readInt(L, "bounty", 1);
            e.armor = readInt(L, "armor", 0);
            e.scale = readFloat(L, "scale", 1.0f);
            e.slowResistant = readBool(L, "slowResistant", false);
            e.isBoss = readBool(L, "isBoss", false);
            e.color = readString(L, "color", "red");
            e.shape = readString(L, "shape", "blob");

            content.enemies.push_back(e);
            lua_pop(L, 1);
        }
    }
    lua_pop(L, 1);

    lua_close(L);
    return content;
}

}  // namespace tf
