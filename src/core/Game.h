// towerfall — the simulation.
//
// This is the whole game as pure state plus an update step. No raylib, no audio,
// no globals: the same class runs in the game loop and in the headless test
// binary, so "the game works" and "the tests pass" cannot drift apart.

#pragma once

#include <string>
#include <vector>

#include "core/Defs.h"
#include "core/Grid.h"
#include "core/Types.h"
#include "core/Waves.h"

namespace tf {

// ------------------------------------------------------------------ entities

struct Tower {
    int id = 0;
    TowerKind kind = TowerKind::Arrow;
    Cell cell;
    int level = 1;                  // 1..3
    float cooldown = 0;             // seconds until it can fire again
    float angle = 0;                // facing, radians
    int hp = 100;
    int maxHp = 100;
    int shieldHp = 0;               // energy shield / fortified barrier
    float totalDamage = 0;          // lifetime damage, for the score screen
    float buffed = 0;               // >0 when a War Beacon is boosting this tower
    float overdriveRemaining = 0;   // >0 when supercharged (2x rate, +range, sparks)
    bool justBuilt = false;         // one-frame flag for the "built" animation
    TargetPriority targetPriority = TargetPriority::First;
};

struct Enemy {
    int id = 0;
    EnemyKind kind = EnemyKind::Grunt;
    EnemyArchetype archetype = EnemyArchetype::Walker;

    float x = 0, y = 0;             // world position (pixels)
    int hp = 1, maxHp = 1;
    float speed = 1;
    int armor = 0;
    int bounty = 0;
    float angle = 0;                // facing direction, radians (0 = East, +pi/2 = South)

    // path following
    std::vector<Cell> path;
    size_t pathIndex = 0;

    // attacking towers
    int targetTowerId = 0;
    float attackCooldown = 0;

    // status effects
    float slowFactor = 1;          // 1 = normal speed, 0.5 = half speed
    float slowTimer = 0;
    float burnTimer = 0, burnDps = 0;
    float poisonTimer = 0, poisonDps = 0;
    float stunTimer = 0;

    float hitFlash = 0;             // seconds of white flash remaining
    float spawnFade = 1;            // 1 -> 0 as it fades in
    bool isBoss = false;
    bool reachedExit = false;

    // Titan Surge Mutators & Abilities
    EnemyAffix affix = EnemyAffix::None;
    float shieldHp = 0;
    float maxShieldHp = 0;
    float abilityCooldown = 6.0f;   // Titan special boss abilities
};

struct Projectile {
    int id = 0;
    int towerId = 0;
    Vec2 pos;
    Vec2 vel;
    float life = 0;
    int damage = 0;
    float splashRadius = 0;
    int chainTargets = 0;
    float slowFactor = 0, slowDuration = 0;
    float statusDuration = 0, statusDps = 0;
    ProjectileStyle style = ProjectileStyle::Arrow;
    int targetId = 0;               // enemy id being chased
    Vec2 targetPos;                 // last known target position
    bool isAura = false;
    float auraDps = 0;
    float auraRadius = 0;
    int pierceCount = 0;
    std::vector<int> alreadyHit;
    bool hit = false;
};

// A floating text or icon that appears and fades.
struct Color4 { float r, g, b, a; };

struct FloatingText {
    Vec2 pos;
    float life = 0;
    float maxLife = 1;
    std::string text;
    Color4 color{1, 1, 1, 1};
};

struct PowerupDrop {
    int id = 0;
    PowerupKind kind = PowerupKind::GoldCache;
    Vec2 pos;
    float life = 14.0f;
    float maxLife = 14.0f;
    float bobOffset = 0.0f;
};

// ------------------------------------------------------------------ events

enum class GameEvent : uint8_t {
    None = 0,
    Built,
    Upgraded,
    Sold,
    Rejected,       // illegal placement
    EnemyLeaked,    // reached the exit
    EnemyKilled,
    TowerDestroyed,
    WaveStarted,
    WaveCleared,
    GameOver,
    Victory,
    BossIncoming,
    TitanSurge,
    TitanAbility,
    TowerRepaired,
    TowerOverdrive,
    MeteorImpact,
    CryoFreezeCast,
    MidasRushCast,
    PowerupCollected,
    ComboTriggered,
};

struct EventLog {
    GameEvent type = GameEvent::None;
    std::string text;
    Vec2 pos;
    int value = 0;
};

// ------------------------------------------------------------------ game

class Game {
public:
    Game();

    // ---- setup
    void loadLevel(const LevelDef& level);
    void setDifficulty(const std::string& d);
    void cycleDifficulty();
    const LevelDef& level() const { return level_; }
    const std::string& difficulty() const { return difficulty_; }

    // ---- state
    int lives() const { return lives_; }
    int gold() const { return gold_; }
    int waveNumber() const { return waveNumber_; }
    int score() const { return score_; }
    bool isGameOver() const { return gameOver_; }
    bool isVictory() const { return victory_; }
    bool isWaveActive() const { return waveActive_; }
    float waveTimer() const { return waveTimer_; }
    float prepTimer() const { return prepTimer_; }
    const std::string& statusMessage() const { return statusMessage_; }
    int titanSurgeTier() const { return tf::titanSurgeTier(waveNumber_); }

    int gameSpeed() const { return gameSpeed_; }
    void setGameSpeed(int s) { gameSpeed_ = std::max(1, std::min(4, s)); }
    void cycleGameSpeed() { gameSpeed_ = (gameSpeed_ % 3) + 1; }

    bool endlessMode() const { return endlessMode_; }
    void setEndlessMode(bool e) { endlessMode_ = e; }
    void toggleEndlessMode() { endlessMode_ = !endlessMode_; }

    bool autoWave() const { return autoWave_; }
    void setAutoWave(bool a) { autoWave_ = a; }
    void toggleAutoWave() { autoWave_ = !autoWave_; }

    const Grid& grid() const { return grid_; }
    const std::vector<Tower>& towers() const { return towers_; }
    const std::vector<Enemy>& enemies() const { return enemies_; }
    const std::vector<Projectile>& projectiles() const { return projectiles_; }
    const std::vector<FloatingText>& floaters() const { return floatingTexts_; }
    const std::vector<EventLog>& events() const { return events_; }
    const std::vector<WaveDef>& waves() const { return waves_; }
    const std::vector<PowerupDrop>& powerups() const { return powerups_; }

    // ---- commander abilities
    float abilityCooldown(CommanderAbility a) const;
    float abilityMaxCooldown(CommanderAbility a) const;
    int abilityCost(CommanderAbility a) const;
    bool canCastAbility(CommanderAbility a) const;
    bool castMeteor(Vec2 worldPos);
    bool castCryoFreeze();
    bool castMidasRush();

    // ---- powerup interaction
    bool collectPowerup(int powerupId);
    bool collectPowerupAt(Vec2 worldPos, float radius = 28.0f);
    void spawnPowerup(Vec2 pos, PowerupKind kind);

    // ---- player actions
    //
    // Can this tower be built here? Returns false and sets a reason when not.
    bool canBuild(TowerKind kind, Cell cell, std::string* reason = nullptr) const;
    bool build(TowerKind kind, Cell cell);

    // Upgrade a tower to the next level. Returns false if it cannot be upgraded.
    bool canUpgrade(int towerId, std::string* reason = nullptr) const;
    bool upgrade(int towerId);
    bool sell(int towerId);
    void cycleTowerTargetPriority(int towerId);
    void setTowerTargetPriority(int towerId, TargetPriority priority);

    // Active tower tactical commands
    bool canRepair(int towerId, int* cost = nullptr) const;
    bool repair(int towerId);
    bool canOverdrive(int towerId, int* cost = nullptr) const;
    bool overdrive(int towerId);

    // ---- simulation
    void update(float dt);
    void startWaveNow();          // "call the wave early" button

    // ---- helpers for the UI
    const Tower* towerAt(Cell c) const;
    const Enemy* enemyById(int id) const;
    Vec2 cellToWorld(Cell c) const;
    Cell worldToCell(Vec2 world) const;
    float tileSize() const { return static_cast<float>(config_.tileSize); }

    // Which towers are inside the beacon's radius (used by the renderer to draw links).
    std::vector<int> towersBuffedBy(int beaconId) const;

    // Damage a tower by an attacker.
    void damageTower(int towerId, int amount);

    // Total enemy kills, for the end screen.
    int kills() const { return kills_; }
    int enemiesLeaked() const { return leaked_; }

    // Cumulative damage taken by towers. A CURRENT hp reading is not enough to
    // tell whether attackers are doing anything, because towers mend between
    // waves — this total is monotonic.
    int towerDamageTaken() const { return towerDamageTaken_; }
    int towersLost() const { return towersLost_; }
    const CombatStats& stats() const { return combatStats_; }

    Cell start() const { return start_; }
    Cell goal() const { return goal_; }
    float effectiveRange(const Tower& t) const;
    int effectiveDamage(const Tower& t) const;

    GameConfig& config() { return config_; }
    const GameConfig& config() const { return config_; }

    // Queue an event for the renderer to consume (particles, sounds).
    void pushEvent(GameEvent type, std::string text = "", Vec2 pos = {0, 0}, int value = 0);

private:
    void spawnEnemy(EnemyKind kind);
    void updateEnemies(float dt);
    void updateTowers(float dt);
    void updateProjectiles(float dt);
    void updateWave(float dt);
    void applyDamage(Enemy& e, int amount, float slowFactor = 0, float slowDuration = 0,
                     float statusDuration = 0, float statusDps = 0, StatusEffect effect = StatusEffect::None);
    void killEnemy(Enemy& e);
    int findTarget(const Tower& t) const;
    void fireAt(Tower& t, Enemy& target);
    void applyImpact(Projectile& p, Enemy& target);
    bool enemyIsTargetable(const Tower& t, const Enemy& e) const;
    void addFloat(const Vec2& pos, const std::string& text, Color4 color);

    Grid grid_;
    LevelDef level_;
    GameConfig config_;
    std::string difficulty_ = "normal";

    std::vector<Tower> towers_;
    std::vector<Enemy> enemies_;
    std::vector<Projectile> projectiles_;
    std::vector<FloatingText> floatingTexts_;
    std::vector<EventLog> events_;
    std::vector<WaveDef> waves_;
    std::vector<PowerupDrop> powerups_;

    Cell start_{0, 0};
    Cell goal_{0, 0};

    int nextTowerId_ = 1;
    int nextEnemyId_ = 1;
    int nextProjectileId_ = 1;
    int nextPowerupId_ = 1;

    int lives_ = 20;
    int gold_ = 260;
    int waveNumber_ = 0;         // 0 = not started
    int score_ = 0;
    int kills_ = 0;
    int leaked_ = 0;
    int towerDamageTaken_ = 0;
    int towersLost_ = 0;

    bool waveActive_ = false;
    bool gameOver_ = false;
    bool victory_ = false;
    float waveTimer_ = 0;        // time since the wave started
    float prepTimer_ = 0;        // time until the next wave
    std::string statusMessage_ = "";

    int gameSpeed_ = 1;          // 1x, 2x, 3x simulation speed
    bool endlessMode_ = false;   // continues past wave 15 into infinite titan escalation
    bool autoWave_ = false;      // automatically triggers next wave when prep finishes
    int lastAnnouncedSurgeTier_ = 0;
    CombatStats combatStats_;
    float abilityCooldowns_[3] = {0.0f, 0.0f, 0.0f};
    float goldRushTimer_ = 0.0f;

    // Spawn schedule: flattened from the wave's groups.
    struct PendingSpawn {
        float at = 0;
        EnemyKind kind = EnemyKind::Grunt;
    };
    std::vector<PendingSpawn> pendingSpawns_;
    size_t nextSpawn_ = 0;

    // Cached path per enemy archetype; recomputed when towers change.
    int pathVersion_ = 0;
};

// The level catalogue.
const std::vector<LevelDef>& allLevels();
const LevelDef& levelByIndex(int index);
int levelCount();

}  // namespace tf
