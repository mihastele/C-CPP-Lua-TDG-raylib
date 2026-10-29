#include "core/Game.h"

#include <algorithm>
#include <cmath>

namespace tf {
namespace {

float clampf(float v, float lo, float hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

}  // namespace

// ------------------------------------------------------------------ update

void Game::update(float dt) {
    if (dt <= 0) return;
    if (gameOver_ || victory_) return;

    updateWave(dt);
    updateEnemies(dt);
    updateTowers(dt);
    updateProjectiles(dt);

    // Floating text lifetimes.
    for (auto& f : floatingTexts_) f.life -= dt;
    floatingTexts_.erase(
        std::remove_if(floatingTexts_.begin(), floatingTexts_.end(),
                       [](const FloatingText& f) { return f.life <= 0; }),
        floatingTexts_.end());
}

void Game::addFloat(const Vec2& pos, const std::string& text, Color4 color) {
    FloatingText f;
    f.pos = pos;
    f.text = text;
    f.color = color;
    f.life = 1.1f;
    f.maxLife = 1.1f;
    floatingTexts_.push_back(f);
}

void Game::pushEvent(GameEvent type, std::string text, Vec2 pos, int value) {
    EventLog e;
    e.type = type;
    e.text = std::move(text);
    e.pos = pos;
    e.value = value;
    events_.push_back(e);
}

// ------------------------------------------------------------------ enemies

void Game::updateEnemies(float dt) {
    const float ts = static_cast<float>(config_.tileSize);

    for (size_t i = 0; i < enemies_.size(); ++i) {
        Enemy& e = enemies_[i];

        if (e.spawnFade > 0) e.spawnFade = std::max(0.0f, e.spawnFade - dt * 2.5f);
        if (e.hitFlash > 0) e.hitFlash = std::max(0.0f, e.hitFlash - dt * 4.0f);

        // ---- status effects
        if (e.stunTimer > 0) {
            e.stunTimer -= dt;
        }
        if (e.slowTimer > 0) {
            e.slowTimer -= dt;
            if (e.slowTimer <= 0) e.slowFactor = 1.0f;
        }
        if (e.burnTimer > 0) {
            e.burnTimer -= dt;
            applyDamage(e, static_cast<int>(e.burnDps * dt), 0, 0, 0, 0, StatusEffect::Burn);
            if (e.hp <= 0) { killEnemy(e); continue; }
        }
        if (e.poisonTimer > 0) {
            e.poisonTimer -= dt;
            applyDamage(e, static_cast<int>(e.poisonDps * dt), 0, 0, 0, 0, StatusEffect::Poison);
            if (e.hp <= 0) { killEnemy(e); continue; }
        }

        // Slow-immune enemies ignore slows entirely.
        const EnemyDef& def = enemyDef(e.kind);
        if (def.slowResistant) e.slowFactor = 1.0f;

        if (e.stunTimer > 0) continue;   // stunned: no movement, no attacking

        // ---- movement
        float speed = e.speed * e.slowFactor;

        if (e.archetype == EnemyArchetype::Walker) {
            // Follow the path. Recompute when the tower layout has changed.
            if (e.path.empty() || e.pathIndex >= e.path.size()) {
                e.path = grid_.findPath(start_, goal_);
                e.pathIndex = 0;
            }
            // Advance along the path.
            bool moved = false;
            while (e.pathIndex < e.path.size()) {
                const Vec2 target = cellToWorld(e.path[e.pathIndex]);
                const Vec2 d = target - Vec2{e.x, e.y};
                const float dist = d.length();
                if (d.lengthSquared() > 0.0001f) {
                    e.angle = std::atan2(d.y, d.x);
                }
                if (dist < speed * dt + 1.0f) {
                    e.x = target.x;
                    e.y = target.y;
                    ++e.pathIndex;
                    moved = true;
                    if (e.pathIndex >= e.path.size()) break;
                } else {
                    const Vec2 step = d.normalized() * (speed * dt);
                    e.x += step.x;
                    e.y += step.y;
                    moved = true;
                    break;
                }
            }
            // Reached the exit?
            const Vec2 goalPos = cellToWorld(goal_);
            if (Vec2{e.x, e.y}.distanceTo(goalPos) < ts * 0.45f) {
                e.reachedExit = true;
                lives_ -= (e.isBoss ? 5 : 1);
                leaked_++;
                score_ = std::max(0, score_ - 20);
                addFloat(goalPos, e.isBoss ? "-5 LIVES" : "-1", {1.0f, 0.35f, 0.35f, 1.0f});
                pushEvent(GameEvent::EnemyLeaked, enemyDef(e.kind).name, Vec2{e.x, e.y});
                if (lives_ <= 0) {
                    lives_ = 0;
                    gameOver_ = true;
                    pushEvent(GameEvent::GameOver, "Your core was destroyed.");
                }
                continue;
            }
            (void)moved;
        } else if (e.archetype == EnemyArchetype::Flyer) {
            // Flyers fly straight to the exit, ignoring the path and the towers.
            const Vec2 goalPos = cellToWorld(goal_);
            const Vec2 d = goalPos - Vec2{e.x, e.y};
            if (d.lengthSquared() > 0.0001f) {
                e.angle = std::atan2(d.y, d.x);
            }
            if (d.length() < speed * dt + 1.0f) {
                e.x = goalPos.x;
                e.y = goalPos.y;
            } else {
                const Vec2 step = d.normalized() * (speed * dt);
                e.x += step.x;
                e.y += step.y;
            }
            if (Vec2{e.x, e.y}.distanceTo(goalPos) < ts * 0.45f) {
                e.reachedExit = true;
                lives_ -= 1;
                leaked_++;
                addFloat(goalPos, "-1", {1.0f, 0.35f, 0.35f, 1.0f});
                pushEvent(GameEvent::EnemyLeaked, enemyDef(e.kind).name, Vec2{e.x, e.y});
                if (lives_ <= 0) { lives_ = 0; gameOver_ = true; }
                continue;
            }
        } else {
            // ---- ATTACKERS: they hunt towers.
            //
            // If they have no target (or their target died), they pick the nearest
            // tower. They walk directly toward it, ignoring the path, and attack
            // it when in range. If there are no towers at all, they head for the
            // exit and leak like a walker.
            Tower* target = nullptr;
            float bestDist = 1e9f;
            for (auto& t : towers_) {
                // Whether a tower can be attacked at all. (Every tower can be
                // attacked; this hook exists so a future "armoured" tower type
                // can opt out.)
                if (!towerDef(t.kind).canTargetAttackers) continue;
                const float d = Vec2{e.x, e.y}.distanceTo(cellToWorld(t.cell));
                if (d < bestDist) { bestDist = d; target = &t; }
            }

            if (!target) {
                // No towers: walk to the exit.
                const Vec2 goalPos = cellToWorld(goal_);
                const Vec2 d = goalPos - Vec2{e.x, e.y};
                if (d.lengthSquared() > 0.0001f) {
                    e.angle = std::atan2(d.y, d.x);
                }
                if (d.length() > 0.5f) {
                    const Vec2 step = d.normalized() * (speed * dt);
                    e.x += step.x;
                    e.y += step.y;
                }
                if (Vec2{e.x, e.y}.distanceTo(goalPos) < ts * 0.45f) {
                    e.reachedExit = true;
                    lives_ -= 1;
                    leaked_++;
                    if (lives_ <= 0) { lives_ = 0; gameOver_ = true; }
                }
                continue;
            }

            const Vec2 targetPos = cellToWorld(target->cell);
            const Vec2 d = targetPos - Vec2{e.x, e.y};
            const float attackRange = def.attackRange * ts;
            if (d.lengthSquared() > 0.0001f) {
                e.angle = std::atan2(d.y, d.x);
            }

            if (d.length() <= attackRange) {
                // In range: attack.
                e.attackCooldown -= dt;
                if (e.attackCooldown <= 0) {
                    e.attackCooldown = 1.0f / std::max(0.1f, def.attackRate);
                    damageTower(target->id, def.damage);
                    addFloat(targetPos, "-" + std::to_string(def.damage),
                             {1.0f, 0.6f, 0.2f, 1.0f});
                }
            } else {
                const Vec2 step = d.normalized() * (speed * dt);
                e.x += step.x;
                e.y += step.y;
            }
        }
    }

    // Remove dead / leaked enemies.
    enemies_.erase(
        std::remove_if(enemies_.begin(), enemies_.end(),
                       [](const Enemy& e) { return e.hp <= 0 || e.reachedExit; }),
        enemies_.end());
}

// ------------------------------------------------------------------ towers

float Game::effectiveRange(const Tower& t) const {
    const TowerDef& def = towerDef(t.kind);
    return def.level(t.level - 1).range * static_cast<float>(config_.tileSize);
}

int Game::effectiveDamage(const Tower& t) const {
    const TowerDef& def = towerDef(t.kind);
    int dmg = def.level(t.level - 1).damage;
    // A War Beacon buffs nearby towers.
    if (t.buffed > 0) dmg = static_cast<int>(dmg * 1.35f);
    return dmg;
}

bool Game::enemyIsTargetable(const Tower& t, const Enemy& e) const {
    const TowerDef& def = towerDef(t.kind);
    const bool flying = e.archetype == EnemyArchetype::Flyer;
    if (flying && !def.targetsAir) return false;
    if (!flying && !def.targetsGround) return false;
    return true;
}

int Game::findTarget(const Tower& t) const {
    const float range = effectiveRange(t);
    const Vec2 origin = cellToWorld(t.cell);
    int best = 0;
    float bestScore = -1e9f;

    for (const auto& e : enemies_) {
        if (!enemyIsTargetable(t, e)) continue;
        const float d = origin.distanceTo(Vec2{e.x, e.y});
        if (d > range) continue;

        // Target selection: prefer whatever is closest to the exit, so enemies
        // that are nearly through get shot first. Bosses get a slight priority
        // boost so they are not ignored behind a crowd.
        const Vec2 goalPos = cellToWorld(goal_);
        const float progress = goalPos.distanceTo(Vec2{e.x, e.y});
        float score = -progress;
        if (e.isBoss) score += 30.0f;
        if (score > bestScore) { bestScore = score; best = e.id; }
    }
    return best;
}

std::vector<int> Game::towersBuffedBy(int beaconId) const {
    std::vector<int> out;
    const Tower* beacon = nullptr;
    for (const auto& t : towers_) if (t.id == beaconId) beacon = &t;
    if (!beacon) return out;
    const TowerDef& def = towerDef(beacon->kind);
    if (beacon->kind != TowerKind::Beacon) return out;
    const float r = def.level(beacon->level - 1).range * static_cast<float>(config_.tileSize);
    const Vec2 bp = cellToWorld(beacon->cell);
    for (const auto& t : towers_) {
        if (t.id == beaconId) continue;
        if (bp.distanceTo(cellToWorld(t.cell)) <= r) out.push_back(t.id);
    }
    return out;
}

void Game::updateTowers(float dt) {
    const float ts = static_cast<float>(config_.tileSize);

    // ---- repair
    //
    // Towers are attacked by Sappers and Bruisers, and that damage has to
    // MATTER — but a defence that can be permanently erased is not a
    // difficulty curve, it is a death spiral: lose your towers, lose the wave,
    // never build them back. So towers mend between waves, and slowly during
    // them. Losing a tower still costs you; losing all of them does not.
    for (auto& t : towers_) {
        if (t.hp >= t.maxHp) continue;
        if (!waveActive_) {
            t.hp = t.maxHp;                       // full repair between waves
        } else {
            t.hp = std::min(t.maxHp, t.hp + static_cast<int>(t.maxHp * 0.04f * dt));
        }
    }

    // Recompute beacon buffs.
    for (auto& t : towers_) t.buffed = 0;
    for (const auto& t : towers_) {
        if (t.kind != TowerKind::Beacon) continue;
        for (int id : towersBuffedBy(t.id)) {
            for (auto& o : towers_) if (o.id == id) o.buffed = 1.0f;
        }
    }

    for (auto& t : towers_) {
        if (t.justBuilt) t.justBuilt = false;
        const TowerDef& def = towerDef(t.kind);
        const TowerLevel& lv = def.level(t.level - 1);

        // ---- Aura towers (Tar Pit) damage everything in range continuously.
        if (def.mode == AttackMode::Aura) {
            if (def.kind == TowerKind::Beacon) continue;   // the beacon deals no damage
            const float r = lv.range * ts;
            const Vec2 origin = cellToWorld(t.cell);
            for (auto& e : enemies_) {
                if (origin.distanceTo(Vec2{e.x, e.y}) > r) continue;
                if (!enemyIsTargetable(t, e)) continue;
                applyDamage(e, static_cast<int>(lv.auraDps * dt),
                            lv.slowFactor, lv.slowDuration, 0, 0, StatusEffect::Slow);
                if (e.hp <= 0) killEnemy(e);
            }
            continue;
        }

        t.cooldown -= dt;
        if (t.cooldown > 0) continue;

        const int targetId = findTarget(t);
        if (targetId == 0) continue;
        const Enemy* te = enemyById(targetId);
        if (!te) continue;

        // Face the target.
        const Vec2 origin = cellToWorld(t.cell);
        const Vec2 toTarget = Vec2{te->x, te->y} - origin;
        if (toTarget.lengthSquared() > 0.0001f) {
            t.angle = std::atan2(toTarget.y, toTarget.x);
        }

        // fireAt mutates the target, so it needs a non-const reference.
        Enemy* mutableTe = nullptr;
        for (auto& e : enemies_) if (e.id == te->id) mutableTe = &e;
        if (!mutableTe) continue;
        fireAt(t, *mutableTe);
        t.cooldown = 1.0f / std::max(0.05f, lv.fireRate);
    }

    // Remove enemies killed by auras.
    enemies_.erase(
        std::remove_if(enemies_.begin(), enemies_.end(),
                       [](const Enemy& e) { return e.hp <= 0 || e.reachedExit; }),
        enemies_.end());
}

void Game::fireAt(Tower& t, Enemy& target) {
    const TowerDef& def = towerDef(t.kind);
    const TowerLevel& lv = def.level(t.level - 1);

    Projectile p;
    p.id = nextProjectileId_++;
    p.towerId = t.id;
    p.pos = cellToWorld(t.cell);
    p.targetId = target.id;
    p.targetPos = Vec2{target.x, target.y};
    p.damage = effectiveDamage(t);
    p.splashRadius = lv.splashRadius * static_cast<float>(config_.tileSize);
    p.chainTargets = lv.chainTargets;
    p.slowFactor = lv.slowFactor;
    p.slowDuration = lv.slowDuration;
    p.statusDuration = lv.statusDuration;
    p.statusDps = lv.statusDps;
    p.pierceCount = lv.pierceCount;
    p.style = def.style;
    p.life = 4.0f;

    const Vec2 dir = (p.targetPos - p.pos).normalized();
    p.vel = dir * def.projectileSpeed * static_cast<float>(config_.tileSize);

    projectiles_.push_back(p);
}

// ------------------------------------------------------------------ projectiles

void Game::updateProjectiles(float dt) {
    const float ts = static_cast<float>(config_.tileSize);

    for (auto& p : projectiles_) {
        if (p.hit) continue;
        p.life -= dt;
        if (p.life <= 0) { p.hit = true; continue; }

        // ---- homing: steer toward the target's current position
        const Enemy* target = enemyById(p.targetId);
        if (target) {
            p.targetPos = Vec2{target->x, target->y};
            const Vec2 d = p.targetPos - p.pos;
            const float dist = d.length();
            // The impact radius must cover a full frame of travel: a fast shot
            // (the Sniper crosses 32px per frame) would otherwise step clean over
            // a fixed 6px circle, miss, and then chase its target in circles for
            // the rest of its life.
            const float hitRadius = std::max(6.0f, p.vel.length() * dt + 2.0f);
            if (dist < hitRadius) {
                // ---- impact
                // applyImpact mutates the enemy, so it needs a non-const reference.
                Enemy* mutableTarget = nullptr;
                for (auto& e : enemies_) if (e.id == target->id) mutableTarget = &e;
                if (mutableTarget) applyImpact(p, *mutableTarget);
                continue;
            }
            const Vec2 want = d.normalized();
            const Vec2 cur = p.vel.lengthSquared() > 1e-6f ? p.vel.normalized() : want;
            // Blend so the projectile curves rather than snapping.
            const Vec2 steer = (want - cur) * clampf(dt * 8.0f, 0, 1);
            p.vel = (cur + steer).normalized() * p.vel.length();
            p.pos += p.vel * dt;
        } else {
            // Target died: the shot has nothing left to hit. Let it fade quickly
            // rather than sailing across the map for the rest of its life.
            p.life = std::min(p.life, 0.25f);
            p.pos += p.vel * dt;
        }
    }

    projectiles_.erase(
        std::remove_if(projectiles_.begin(), projectiles_.end(),
                       [](const Projectile& p) { return p.hit; }),
        projectiles_.end());
}

void Game::applyImpact(Projectile& p, Enemy& target) {
    const TowerDef& def = towerDef(towers_.empty() ? TowerKind::Arrow : towers_[0].kind);
    (void)def;

    // ---- direct damage
    applyDamage(target, p.damage, p.slowFactor, p.slowDuration,
                p.statusDuration, p.statusDps, StatusEffect::None);
    target.hitFlash = 0.12f;

    // ---- splash
    if (p.splashRadius > 0) {
        for (auto& e : enemies_) {
            if (e.id == target.id) continue;
            const float d = Vec2{e.x, e.y}.distanceTo(Vec2{target.x, target.y});
            if (d > p.splashRadius) continue;
            const float falloff = 1.0f - (d / p.splashRadius) * 0.5f;
            applyDamage(e, static_cast<int>(p.damage * falloff), p.slowFactor, p.slowDuration,
                        p.statusDuration, p.statusDps, StatusEffect::None);
            e.hitFlash = 0.1f;
        }
    }

    // ---- pierce: keep going through additional enemies
    if (p.pierceCount > 0) {
        p.alreadyHit.push_back(target.id);
        bool foundNext = false;
        for (auto& e : enemies_) {
            if (std::find(p.alreadyHit.begin(), p.alreadyHit.end(), e.id) != p.alreadyHit.end())
                continue;
            const float d = Vec2{e.x, e.y}.distanceTo(p.pos);
            if (d < 18.0f) {
                applyDamage(e, p.damage, p.slowFactor, p.slowDuration,
                            p.statusDuration, p.statusDps, StatusEffect::None);
                e.hitFlash = 0.1f;
                p.alreadyHit.push_back(e.id);
                foundNext = true;
                if (static_cast<int>(p.alreadyHit.size()) > p.pierceCount) break;
            }
        }
        if (!foundNext) p.hit = true;
        else {
            // Continue in the current direction.
            p.targetId = 0;
            p.life = std::min(p.life, 0.35f);
        }
        return;
    }

    // ---- chain: bounce to nearby enemies
    if (p.chainTargets > 0) {
        int hops = p.chainTargets;
        int currentId = target.id;
        std::vector<int> hitIds{currentId};
        int lastDamage = p.damage;
        while (hops-- > 0) {
            const Enemy* from = enemyById(currentId);
            if (!from) break;
            const Vec2 fromPos{from->x, from->y};
            int nextId = 0;
            float bestDist = 2.2f * static_cast<float>(config_.tileSize);
            for (const auto& e : enemies_) {
                if (std::find(hitIds.begin(), hitIds.end(), e.id) != hitIds.end()) continue;
                const float d = fromPos.distanceTo(Vec2{e.x, e.y});
                if (d < bestDist) { bestDist = d; nextId = e.id; }
            }
            if (!nextId) break;
            Enemy* next = nullptr;
            for (auto& e : enemies_) if (e.id == nextId) next = &e;
            if (!next) break;
            lastDamage = static_cast<int>(lastDamage * 0.75f);
            applyDamage(*next, lastDamage, p.slowFactor, p.slowDuration,
                        p.statusDuration, p.statusDps, StatusEffect::None);
            next->hitFlash = 0.1f;
            hitIds.push_back(nextId);
            currentId = nextId;
        }
    }

    p.hit = true;
}

void Game::applyDamage(Enemy& e, int amount, float slowFactor, float slowDuration,
                        float statusDuration, float statusDps, StatusEffect effect) {
    if (e.hp <= 0) return;
    // Armour is a PERCENTAGE reduction, not a flat subtraction.
    //
    // Flat armour was a balance trap: a Colossus with 12 armour turned an
    // Archer's 9 damage into 1, so a whole tower type did nothing to bosses.
    // A percentage keeps armour meaningful without making a tower useless.
    int dmg = amount;
    if (e.armor > 0) {
        dmg = static_cast<int>(amount * (1.0f - std::min(0.75f, e.armor / 100.0f)));
    }
    if (dmg < 1) dmg = 1;   // armour never makes a hit a total miss
    e.hp -= dmg;

    if (effect == StatusEffect::Burn && statusDuration > 0) {
        e.burnTimer = std::max(e.burnTimer, statusDuration);
        e.burnDps = std::max(e.burnDps, statusDps);
    }
    if (effect == StatusEffect::Poison && statusDuration > 0) {
        e.poisonTimer = std::max(e.poisonTimer, statusDuration);
        e.poisonDps = std::max(e.poisonDps, statusDps);
    }
    if (slowFactor > 0 && slowDuration > 0) {
        // Slow-immune enemies ignore it (handled in updateEnemies).
        e.slowFactor = std::min(e.slowFactor, slowFactor);
        e.slowTimer = std::max(e.slowTimer, slowDuration);
    }

    // A killing blow must go through killEnemy, not just reduce hp: that is
    // where the bounty, the score and the kill counter are awarded. Without
    // this, enemies vanished but the player was never paid.
    if (e.hp <= 0) killEnemy(e);
}

void Game::killEnemy(Enemy& e) {
    if (e.hp > 0) return;
    e.hp = 0;
    gold_ += e.bounty;
    score_ += e.isBoss ? 500 : (10 + e.bounty * 2);
    kills_++;
    addFloat(Vec2{e.x, e.y}, "+" + std::to_string(e.bounty),
             {1.0f, 0.85f, 0.3f, 1.0f});
    pushEvent(GameEvent::EnemyKilled, enemyDef(e.kind).name, Vec2{e.x, e.y}, e.bounty);
}

}  // namespace tf
