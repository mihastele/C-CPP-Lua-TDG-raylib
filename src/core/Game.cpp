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
    if (d == "easy")  { hp = 0.75f; speed = 0.92f; gold = 320; lives = 30; }
    else if (d == "hard") { hp = 1.45f; speed = 1.12f; gold = 220; lives = 15; }
    else               { hp = 1.0f;  speed = 1.0f;  gold = 260; lives = 20; }
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

    setDifficulty(level.difficulty);

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
        grid_.setTower(towers_[i].cell, 0);
        pushEvent(GameEvent::Sold, def.name, cellToWorld(towers_[i].cell), refund);
        statusMessage_ = "Sold for " + std::to_string(refund) + " gold.";
        towers_.erase(towers_.begin() + static_cast<long>(i));
        pathVersion_++;
        return true;
    }
    return false;
}

void Game::damageTower(int towerId, int amount) {
    for (auto& t : towers_) {
        if (t.id != towerId) continue;
        t.hp -= amount;
        towerDamageTaken_ += amount;
        if (t.hp <= 0) {
            ++towersLost_;
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
    if (gameOver_ || victory_) return;

    if (!waveActive_) {
        prepTimer_ -= dt;
        if (prepTimer_ <= 0) {
            // ---- start the next wave
            waveNumber_++;
            if (waveNumber_ > static_cast<int>(waves_.size())) {
                victory_ = true;
                pushEvent(GameEvent::Victory, "All waves cleared!");
                return;
            }
            const WaveDef& w = waves_[static_cast<size_t>(waveNumber_ - 1)];
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
            statusMessage_ = w.name + " incoming!";
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

    Enemy e;
    e.id = nextEnemyId_++;
    e.kind = kind;
    e.archetype = def.archetype;
    e.maxHp = e.hp = std::max(1, static_cast<int>(def.hp * hpScale));
    // Speed is authored in TILES per second but the simulation works in pixels, so
    // convert once here rather than at every movement site.
    e.speed = def.speed * speedScale * static_cast<float>(config_.tileSize);
    e.armor = def.armor;
    e.bounty = def.bounty;
    e.isBoss = def.isBoss;
    e.x = cellToWorld(start_).x;
    e.y = cellToWorld(start_).y;

    enemies_.push_back(e);
}

}  // namespace tf
