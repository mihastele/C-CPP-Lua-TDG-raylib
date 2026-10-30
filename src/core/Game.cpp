#include "core/Game.h"

#include <algorithm>
#include <cmath>

namespace tf {
namespace {

constexpr float kEpsilon = 0.001f;

float clampf(float v, float lo, float hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

// Difficulty multipliers, mirroring Waves.cpp.
void difficultyScale(const std::string& d, float& hp, float& speed, int& gold, int& lives) {
    if (d == "easy")           { hp = 0.75f; speed = 0.92f; gold = 320; lives = 30; }
    else if (d == "hard")      { hp = 1.45f; speed = 1.12f; gold = 220; lives = 15; }
    else if (d == "nightmare") { hp = 2.0f;  speed = 1.25f; gold = 200; lives = 10; }
    else                       { hp = 1.0f;  speed = 1.0f;  gold = 260; lives = 20; }
}

}  // namespace

// ------------------------------------------------------------------ levels

const LevelDef& levelByIndex(int index) {
    const auto& all = allLevels();
    if (index < 0) return all.front();
    if (index >= static_cast<int>(all.size())) return all.back();
    return all[static_cast<size_t>(index)];
}

int levelCount() { return static_cast<int>(allLevels().size()); }

// ------------------------------------------------------------------ game

Game::Game() {
    config_.gridW = 24;
    config_.gridH = 14;
    config_.tileSize = 48;
}

void Game::setDifficulty(const std::string& d) {
    difficulty_ = d;
    float hp = 1, sp = 1;
    int gold = 260, lives = 20;
    difficultyScale(d, hp, sp, gold, lives);
    gold_ = gold;
    lives_ = lives;
    config_.startingGold = gold;
    config_.startingLives = lives;
    waves_ = buildWaves(d, 15);
}

void Game::cycleDifficulty() {
    if (difficulty_ == "easy") setDifficulty("normal");
    else if (difficulty_ == "normal") setDifficulty("hard");
    else if (difficulty_ == "hard") setDifficulty("nightmare");
    else setDifficulty("easy");
    statusMessage_ = "Difficulty set to " + difficulty_;
}

void Game::loadLevel(const LevelDef& level) {
    level_ = level;
    Cell s, e;
    grid_ = Grid::fromAscii(level.rows, &s, &e);
    // Mark the intended route so the player can see it and so enemies prefer
    // to walk it. The route is a suggestion: enemies re-path around towers.
    level_.applyRoute(&grid_);
    start_ = s;
    goal_ = e;

    towers_.clear();
    enemies_.clear();
    projectiles_.clear();
    floatingTexts_.clear();
    events_.clear();
    pendingSpawns_.clear();
    nextSpawn_ = 0;
    nextTowerId_ = 1;
    nextEnemyId_ = 1;
    nextProjectileId_ = 1;

    waveNumber_ = 0;
    score_ = 0;
    kills_ = 0;
    leaked_ = 0;
    towerDamageTaken_ = 0;
    towersLost_ = 0;
    waveActive_ = false;
    gameOver_ = false;
    victory_ = false;
    waveTimer_ = 0;
    prepTimer_ = config_.prepTime;
    pathVersion_ = 0;
    gameSpeed_ = 1;
    lastAnnouncedSurgeTier_ = 0;

    setDifficulty(level.difficulty);
    combatStats_ = CombatStats{};
    powerups_.clear();
    nextPowerupId_ = 1;
    abilityCooldowns_[0] = abilityCooldowns_[1] = abilityCooldowns_[2] = 0.0f;
    goldRushTimer_ = 0.0f;

    // Mark the intended route for drawing: the straight-ish path the enemies
    // will take when nothing is built.
    std::vector<Cell> route = grid_.findPath(start_, goal_, true);
    grid_.setPathTiles(route);
}

Vec2 Game::cellToWorld(Cell c) const {
    const float ts = static_cast<float>(config_.tileSize);
    return Vec2{static_cast<float>(c.x) * ts + ts * 0.5f,
                static_cast<float>(c.y) * ts + ts * 0.5f};
}

Cell Game::worldToCell(Vec2 world) const {
    const float ts = static_cast<float>(config_.tileSize);
    return Cell{static_cast<int>(std::floor(world.x / ts)),
                static_cast<int>(std::floor(world.y / ts))};
}

const Tower* Game::towerAt(Cell c) const {
    for (const auto& t : towers_) if (t.cell == c) return &t;
    return nullptr;
}

const Enemy* Game::enemyById(int id) const {
    for (const auto& e : enemies_) if (e.id == id) return &e;
    return nullptr;
}

// ------------------------------------------------------------------ building

bool Game::canBuild(TowerKind kind, Cell cell, std::string* reason) const {
    auto fail = [&](const char* m) { if (reason) *reason = m; return false; };

    if (gameOver_ || victory_) return fail("The game is over.");
    if (!grid_.inBounds(cell)) return fail("Outside the map.");
    if (!grid_.isBuildableTerrain(cell)) return fail("That ground cannot be built on.");
    if (grid_.hasTower(cell)) return fail("Something is already there.");
    if (grid_.towerAt(cell) != 0) return fail("Something is already there.");

    const TowerDef& def = towerDef(kind);
    const int cost = def.level(0).cost;
    if (gold_ < cost) {
        if (reason) *reason = "Not enough gold.";
        return false;
    }

    // THE CORE RULE: a tower may be placed anywhere — including on the path —
    // but it must never make the path impassable.
    //
    // This is checked by imagining the tower on this cell and asking the
    // pathfinder whether an enemy could still get from the entry to the exit.
    Grid probe = grid_;
    probe.setTower(cell, 9999);
    if (!probe.canReach(start_, goal_)) {
        return fail("That would block the path completely.");
    }

    // Flyers ignore the path, so blocking it for flyers is not the concern —
    // but ground enemies must always have a route, which the check above ensures.
    return true;
}

bool Game::build(TowerKind kind, Cell cell) {
    std::string why;
    if (!canBuild(kind, cell, &why)) {
        pushEvent(GameEvent::Rejected, why, cellToWorld(cell));
        statusMessage_ = why;
        return false;
    }

    const TowerDef& def = towerDef(kind);
    const TowerLevel& lv = def.level(0);

    Tower t;
    t.id = nextTowerId_++;
    t.kind = kind;
    t.cell = cell;
    t.level = 1;
    t.hp = 100;
    t.maxHp = 100;
    t.justBuilt = true;
    towers_.push_back(t);
    grid_.setTower(cell, t.id);

    gold_ -= lv.cost;
    combatStats_.towersBuilt++;
    combatStats_.totalGoldSpent += lv.cost;
    score_ += 5;
    pushEvent(GameEvent::Built, def.name, cellToWorld(cell), lv.cost);
    statusMessage_ = def.name + " built.";
    pathVersion_++;   // enemies may need to re-route
    return true;
}

bool Game::canUpgrade(int towerId, std::string* reason) const {
    auto fail = [&](const char* m) { if (reason) *reason = m; return false; };
    const Tower* t = nullptr;
    for (const auto& x : towers_) if (x.id == towerId) t = &x;
    if (!t) return fail("No such tower.");
    const TowerDef& def = towerDef(t->kind);
    if (t->level >= def.maxLevel()) return fail("Already at maximum level.");
    const int cost = def.level(t->level).cost;
    if (gold_ < cost) {
        if (reason) *reason = "Not enough gold.";
        return false;
    }
    return true;
}

bool Game::upgrade(int towerId) {
    std::string why;
    if (!canUpgrade(towerId, &why)) {
        statusMessage_ = why;
        pushEvent(GameEvent::Rejected, why);
        return false;
    }
    for (auto& t : towers_) {
        if (t.id != towerId) continue;
        const TowerDef& def = towerDef(t.kind);
        const int cost = def.level(t.level).cost;
        gold_ -= cost;
        combatStats_.totalGoldSpent += cost;
        t.level++;
        t.hp = t.maxHp = 100 + (t.level - 1) * 50;
        score_ += 10;
        pushEvent(GameEvent::Upgraded, def.name + " Lv" + std::to_string(t.level),
                  cellToWorld(t.cell), cost);
        statusMessage_ = def.name + " upgraded to level " + std::to_string(t.level) + ".";
        return true;
    }
    return false;
}

bool Game::sell(int towerId) {
    for (size_t i = 0; i < towers_.size(); ++i) {
        if (towers_[i].id != towerId) continue;
        const TowerDef& def = towerDef(towers_[i].kind);
        // Refund 60% of everything invested in this tower.
        int invested = 0;
        for (int l = 0; l < towers_[i].level; ++l) invested += def.level(l).cost;
        const int refund = invested * 60 / 100;
        gold_ += refund;
        combatStats_.totalGoldEarned += refund;
        grid_.setTower(towers_[i].cell, 0);
        pushEvent(GameEvent::Sold, def.name, cellToWorld(towers_[i].cell), refund);
        statusMessage_ = "Sold for " + std::to_string(refund) + " gold.";
        towers_.erase(towers_.begin() + static_cast<long>(i));
        pathVersion_++;
        return true;
    }
    return false;
}

void Game::cycleTowerTargetPriority(int towerId) {
    for (auto& t : towers_) {
        if (t.id != towerId) continue;
        int next = (static_cast<int>(t.targetPriority) + 1) % 4;
        t.targetPriority = static_cast<TargetPriority>(next);
        statusMessage_ = towerDef(t.kind).name + " targeting: " + targetPriorityName(t.targetPriority);
        return;
    }
}

void Game::setTowerTargetPriority(int towerId, TargetPriority priority) {
    for (auto& t : towers_) {
        if (t.id != towerId) continue;
        t.targetPriority = priority;
        return;
    }
}

bool Game::canRepair(int towerId, int* costOut) const {
    const Tower* t = nullptr;
    for (const auto& x : towers_) if (x.id == towerId) t = &x;
    if (!t) return false;
    const int cost = 20;
    if (costOut) *costOut = cost;
    if (t->hp >= t->maxHp && t->shieldHp >= 50) return false;
    return gold_ >= cost;
}

bool Game::repair(int towerId) {
    int cost = 0;
    if (!canRepair(towerId, &cost)) return false;
    for (auto& t : towers_) {
        if (t.id != towerId) continue;
        gold_ -= cost;
        combatStats_.totalGoldSpent += cost;
        t.hp = t.maxHp;
        t.shieldHp = 50;
        pushEvent(GameEvent::TowerRepaired, "Repaired & Shielded", cellToWorld(t.cell), cost);
        statusMessage_ = towerDef(t.kind).name + " repaired and fortified!";
        return true;
    }
    return false;
}

bool Game::canOverdrive(int towerId, int* costOut) const {
    const Tower* t = nullptr;
    for (const auto& x : towers_) if (x.id == towerId) t = &x;
    if (!t) return false;
    const int cost = 45;
    if (costOut) *costOut = cost;
    if (t->level < 3) return false;
    if (t->overdriveRemaining > 0) return false;
    return gold_ >= cost;
}

bool Game::overdrive(int towerId) {
    int cost = 0;
    if (!canOverdrive(towerId, &cost)) return false;
    for (auto& t : towers_) {
        if (t.id != towerId) continue;
        gold_ -= cost;
        combatStats_.totalGoldSpent += cost;
        t.overdriveRemaining = 8.5f;
        pushEvent(GameEvent::TowerOverdrive, "OVERDRIVE!", cellToWorld(t.cell), cost);
        statusMessage_ = towerDef(t.kind).name + " entered OVERDRIVE!";
        return true;
    }
    return false;
}

float Game::abilityCooldown(CommanderAbility a) const {
    size_t idx = static_cast<size_t>(a);
    if (idx < 3) return std::max(0.0f, abilityCooldowns_[idx]);
    return 0.0f;
}

float Game::abilityMaxCooldown(CommanderAbility a) const {
    switch (a) {
        case CommanderAbility::Meteor:     return 14.0f;
        case CommanderAbility::CryoFreeze: return 20.0f;
        case CommanderAbility::MidasRush:  return 22.0f;
        default: return 10.0f;
    }
}

int Game::abilityCost(CommanderAbility a) const {
    switch (a) {
        case CommanderAbility::Meteor:     return 35;
        case CommanderAbility::CryoFreeze: return 40;
        case CommanderAbility::MidasRush:  return 45;
        default: return 50;
    }
}

bool Game::canCastAbility(CommanderAbility a) const {
    if (gameOver_ || (victory_ && !endlessMode_)) return false;
    size_t idx = static_cast<size_t>(a);
    if (idx >= 3) return false;
    if (abilityCooldowns_[idx] > 0.0f) return false;
    return gold_ >= abilityCost(a);
}

bool Game::castMeteor(Vec2 worldPos) {
    if (!canCastAbility(CommanderAbility::Meteor)) return false;
    const int cost = abilityCost(CommanderAbility::Meteor);
    gold_ -= cost;
    combatStats_.totalGoldSpent += cost;
    combatStats_.commanderAbilitiesCast++;
    abilityCooldowns_[static_cast<size_t>(CommanderAbility::Meteor)] = abilityMaxCooldown(CommanderAbility::Meteor);

    pushEvent(GameEvent::MeteorImpact, "METEOR STRIKE!", worldPos, 500);
    addFloat(worldPos, "METEOR STRIKE!", {1.0f, 0.45f, 0.15f, 1.0f});

    const float radius = 2.5f * static_cast<float>(config_.tileSize);
    for (auto& e : enemies_) {
        if (worldPos.distanceTo(Vec2{e.x, e.y}) <= radius) {
            applyDamage(e, 500, 0, 0, 4.0f, 50.0f, StatusEffect::Burn);
        }
    }
    statusMessage_ = "Orbital meteor strike delivered!";
    return true;
}

bool Game::castCryoFreeze() {
    if (!canCastAbility(CommanderAbility::CryoFreeze)) return false;
    const int cost = abilityCost(CommanderAbility::CryoFreeze);
    gold_ -= cost;
    combatStats_.totalGoldSpent += cost;
    combatStats_.commanderAbilitiesCast++;
    abilityCooldowns_[static_cast<size_t>(CommanderAbility::CryoFreeze)] = abilityMaxCooldown(CommanderAbility::CryoFreeze);

    pushEvent(GameEvent::CryoFreezeCast, "BLIZZARD FREEZE!", cellToWorld(start_));
    for (auto& e : enemies_) {
        e.stunTimer = std::max(e.stunTimer, 3.5f);
        e.slowFactor = 0.0f;
    }
    addFloat(cellToWorld(start_), "BLIZZARD FREEZE!", {0.4f, 0.85f, 1.0f, 1.0f});
    addFloat(cellToWorld(goal_), "FROZEN!", {0.4f, 0.85f, 1.0f, 1.0f});
    statusMessage_ = "Cryo Blizzard froze all enemies!";
    return true;
}

bool Game::castMidasRush() {
    if (!canCastAbility(CommanderAbility::MidasRush)) return false;
    const int cost = abilityCost(CommanderAbility::MidasRush);
    gold_ -= cost;
    combatStats_.totalGoldSpent += cost;
    combatStats_.commanderAbilitiesCast++;
    abilityCooldowns_[static_cast<size_t>(CommanderAbility::MidasRush)] = abilityMaxCooldown(CommanderAbility::MidasRush);

    pushEvent(GameEvent::MidasRushCast, "MIDAS GOLD RUSH!", cellToWorld(goal_));
    goldRushTimer_ = 8.0f;

    std::vector<int> targetIds;
    for (const auto& e : enemies_) {
        if (!e.isBoss && e.hp > 0) targetIds.push_back(e.id);
    }
    std::sort(targetIds.begin(), targetIds.end(), [this](int a, int b) {
        const Enemy* ea = enemyById(a);
        const Enemy* eb = enemyById(b);
        if (!ea || !eb) return false;
        return ea->hp < eb->hp;
    });

    int executed = 0;
    for (int id : targetIds) {
        if (executed >= 5) break;
        for (auto& e : enemies_) {
            if (e.id == id && e.hp > 0) {
                e.hp = 0;
                gold_ += e.bounty + 20;
                combatStats_.totalGoldEarned += e.bounty + 20;
                addFloat(Vec2{e.x, e.y}, "TRANSMUTED! +" + std::to_string(e.bounty + 20), {1.0f, 0.85f, 0.2f, 1.0f});
                killEnemy(e);
                executed++;
                break;
            }
        }
    }
    addFloat(cellToWorld(goal_), "GOLD RUSH ACTIVE (+50% BOUNTY)", {1.0f, 0.85f, 0.2f, 1.0f});
    statusMessage_ = "Midas Gold Rush active!";
    return true;
}

void Game::spawnPowerup(Vec2 pos, PowerupKind kind) {
    if (kind == PowerupKind::None) return;
    PowerupDrop p;
    p.id = nextPowerupId_++;
    p.kind = kind;
    p.pos = pos;
    powerups_.push_back(p);
}

bool Game::collectPowerup(int powerupId) {
    for (size_t i = 0; i < powerups_.size(); ++i) {
        if (powerups_[i].id != powerupId) continue;
        PowerupDrop p = powerups_[i];
        powerups_.erase(powerups_.begin() + i);

        combatStats_.powerupsCollected++;
        pushEvent(GameEvent::PowerupCollected, powerupName(p.kind), p.pos);

        switch (p.kind) {
            case PowerupKind::GoldCache:
                gold_ += 30;
                combatStats_.totalGoldEarned += 30;
                addFloat(p.pos, "+30 GOLD", {1.0f, 0.85f, 0.2f, 1.0f});
                break;
            case PowerupKind::OverchargeAll:
                for (auto& t : towers_) {
                    t.overdriveRemaining = std::max(t.overdriveRemaining, 6.0f);
                }
                addFloat(p.pos, "ALL TOWERS OVERCHARGED!", {1.0f, 0.8f, 0.2f, 1.0f});
                break;
            case PowerupKind::CryoFreeze:
                for (auto& e : enemies_) {
                    e.stunTimer = std::max(e.stunTimer, 2.5f);
                }
                addFloat(p.pos, "CRYO BLIZZARD!", {0.4f, 0.9f, 1.0f, 1.0f});
                break;
            case PowerupKind::CoreLife:
                lives_ = std::min(config_.startingLives, lives_ + 1);
                addFloat(p.pos, "+1 CORE LIFE", {1.0f, 0.3f, 0.4f, 1.0f});
                break;
            default: break;
        }
        return true;
    }
    return false;
}

bool Game::collectPowerupAt(Vec2 worldPos, float radius) {
    for (const auto& p : powerups_) {
        if (worldPos.distanceTo(p.pos) <= radius) {
            return collectPowerup(p.id);
        }
    }
    return false;
}

void Game::damageTower(int towerId, int amount) {
    for (auto& t : towers_) {
        if (t.id != towerId) continue;
        int remaining = amount;
        if (t.shieldHp > 0) {
            int absorbed = std::min(t.shieldHp, remaining);
            t.shieldHp -= absorbed;
            remaining -= absorbed;
        }
        if (remaining > 0) {
            t.hp -= remaining;
            towerDamageTaken_ += remaining;
        }
        if (t.hp <= 0) {
            ++towersLost_;
            combatStats_.towersLost++;
            pushEvent(GameEvent::TowerDestroyed, towerDef(t.kind).name, cellToWorld(t.cell));
            statusMessage_ = towerDef(t.kind).name + " was destroyed!";
            grid_.setTower(t.cell, 0);
            towers_.erase(towers_.begin() + static_cast<long>(&t - &towers_[0]));
            pathVersion_++;
        }
        return;
    }
}

// ------------------------------------------------------------------ waves

void Game::startWaveNow() {
    if (waveActive_ || gameOver_ || victory_) return;
    prepTimer_ = 0;
}

void Game::updateWave(float dt) {
    if (gameOver_) return;
    if (victory_ && !endlessMode_) return;

    if (!waveActive_) {
        // Auto-wave trigger if enabled
        if (autoWave_ && prepTimer_ > 0.4f) {
            prepTimer_ = 0.4f;
        }

        prepTimer_ -= dt;
        if (prepTimer_ <= 0) {
            // ---- start the next wave
            waveNumber_++;
            if (waveNumber_ > static_cast<int>(waves_.size())) {
                if (endlessMode_) {
                    waves_.push_back(generateWave(waveNumber_, difficulty_));
                } else {
                    victory_ = true;
                    pushEvent(GameEvent::Victory, "All waves cleared!");
                    statusMessage_ = "Victory! Press [E] for Endless Titan Escalation!";
                    return;
                }
            }

            const WaveDef& w = waves_[static_cast<size_t>(waveNumber_ - 1)];

            // Check if Titan Surge Tier escalated
            int curTier = tf::titanSurgeTier(waveNumber_);
            combatStats_.maxSurgeTier = std::max(combatStats_.maxSurgeTier, curTier);
            if (curTier > lastAnnouncedSurgeTier_) {
                lastAnnouncedSurgeTier_ = curTier;
                pushEvent(GameEvent::TitanSurge, std::string(titanSurgeName(curTier)) + " UNLEASHED!", {0, 0}, curTier);
                addFloat(cellToWorld(start_), std::string(titanSurgeName(curTier)) + "!", {1.0f, 0.25f, 0.25f, 1.0f});
            }

            pendingSpawns_.clear();
            nextSpawn_ = 0;
            for (const auto& g : w.groups) {
                for (int i = 0; i < g.count; ++i) {
                    pendingSpawns_.push_back(PendingSpawn{g.delay + i * g.interval, g.kind});
                }
            }
            std::sort(pendingSpawns_.begin(), pendingSpawns_.end(),
                      [](const PendingSpawn& a, const PendingSpawn& b) { return a.at < b.at; });
            waveActive_ = true;
            waveTimer_ = 0;
            pushEvent(GameEvent::WaveStarted, w.name, {0, 0}, w.number);
            if (w.boss) pushEvent(GameEvent::BossIncoming, w.name);
            statusMessage_ = w.name + (curTier > 0 ? " [" + std::string(titanSurgeName(curTier)) + "]" : "") + " incoming!";
        }
        return;
    }

    // ---- active wave: release scheduled spawns
    waveTimer_ += dt;
    while (nextSpawn_ < pendingSpawns_.size() &&
           pendingSpawns_[nextSpawn_].at <= waveTimer_) {
        spawnEnemy(pendingSpawns_[nextSpawn_].kind);
        ++nextSpawn_;
    }

    // ---- wave cleared when nothing is left
    if (nextSpawn_ >= pendingSpawns_.size() && enemies_.empty()) {
        waveActive_ = false;
        const WaveDef& w = waves_[static_cast<size_t>(waveNumber_ - 1)];
        gold_ += w.reward;
        score_ += 100 + w.number * 25;
        prepTimer_ = config_.waveDelay + w.prepBonus;
        pushEvent(GameEvent::WaveCleared, w.name + " cleared!", {0, 0}, w.reward);
        statusMessage_ = w.name + " cleared! +" + std::to_string(w.reward) + " gold";
    }
}

void Game::spawnEnemy(EnemyKind kind) {
    const EnemyDef& def = enemyDef(kind);

    float hpScale = 1, speedScale = 1;
    int gold = 260, lives = 20;
    difficultyScale(difficulty_, hpScale, speedScale, gold, lives);

    TitanSurgeStats surge = calcTitanSurge(waveNumber_, difficulty_);

    Enemy e;
    e.id = nextEnemyId_++;
    e.kind = kind;
    e.archetype = def.archetype;
    // Exponential HP scaling every 4 waves after the first titan spawn:
    e.maxHp = e.hp = std::max(1, static_cast<int>(def.hp * hpScale * surge.hpMultiplier));
    // Speed is authored in TILES per second but the simulation works in pixels:
    e.speed = def.speed * speedScale * surge.speedMultiplier * static_cast<float>(config_.tileSize);
    e.armor = std::min(75, def.armor + surge.armorBonus);
    e.bounty = std::max(1, static_cast<int>(def.bounty * (1.0f + 0.35f * surge.tier)));
    e.isBoss = def.isBoss;
    e.x = cellToWorld(start_).x;
    e.y = cellToWorld(start_).y;
    const Vec2 goalPos = cellToWorld(goal_);
    const Vec2 initialDir = goalPos - Vec2{e.x, e.y};
    if (initialDir.lengthSquared() > 0.0001f) {
        e.angle = std::atan2(initialDir.y, initialDir.x);
    }

    // Assign Elite Affixes based on Titan Surge Tier
    if (surge.tier >= 1) {
        if (e.isBoss) {
            // Bosses always get an elite trait in Surge Tiers
            int bossPick = 1 + (e.id % 3); // Frenzied, Juggernaut, or Shielded
            e.affix = static_cast<EnemyAffix>(bossPick);
        } else {
            // Non-bosses roll chance based on tier
            float roll = static_cast<float>((e.id * 7919) % 1000) / 1000.0f;
            if (roll < surge.affixChance) {
                int pick = 1 + ((e.id * 31) % 5); // 1..5
                e.affix = static_cast<EnemyAffix>(pick);
            }
        }

        // Apply immediate affix attributes
        if (e.affix == EnemyAffix::Frenzied) {
            e.speed *= 1.35f;
        } else if (e.affix == EnemyAffix::Juggernaut) {
            e.maxHp = e.hp = static_cast<int>(e.hp * 1.5f);
            e.armor = std::min(80, e.armor + 4);
        } else if (e.affix == EnemyAffix::Shielded) {
            e.shieldHp = e.maxShieldHp = static_cast<float>(e.maxHp * 0.5f);
        }
    }

    enemies_.push_back(e);
}

}  // namespace tf
