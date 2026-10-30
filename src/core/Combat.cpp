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
    if (gameOver_ || (victory_ && !endlessMode_)) return;

    const float effectiveDt = dt * static_cast<float>(gameSpeed_);

    updateWave(effectiveDt);
    updateEnemies(effectiveDt);
    updateTowers(effectiveDt);
    updateProjectiles(effectiveDt);

    // Update commander ability cooldowns and Midas gold rush timer
    for (int i = 0; i < 3; ++i) {
        if (abilityCooldowns_[i] > 0) abilityCooldowns_[i] -= effectiveDt;
    }
    if (goldRushTimer_ > 0) goldRushTimer_ -= effectiveDt;

    // Update powerup lifetimes and auto-vacuum by nearby towers
    const float ts = static_cast<float>(config_.tileSize);
    for (size_t i = 0; i < powerups_.size(); ) {
        powerups_[i].life -= effectiveDt;
        powerups_[i].bobOffset += effectiveDt * 3.5f;

        // Auto-vacuum: if any tower is within 1.3 tiles, tower collects it!
        bool collected = false;
        for (const auto& t : towers_) {
            if (cellToWorld(t.cell).distanceTo(powerups_[i].pos) <= ts * 1.35f) {
                collectPowerup(powerups_[i].id);
                collected = true;
                break;
            }
        }
        if (collected) {
            continue;
        }

        if (powerups_[i].life <= 0) {
            powerups_.erase(powerups_.begin() + i);
        } else {
            ++i;
        }
    }

    // Floating text lifetimes.
    for (auto& f : floatingTexts_) f.life -= effectiveDt;
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

        // Elite Affix: Regenerator heals HP over time
        if (e.affix == EnemyAffix::Regenerator && e.hp < e.maxHp && e.hp > 0) {
            const int heal = std::max(1, static_cast<int>(e.maxHp * 0.04f * dt));
            e.hp = std::min(e.maxHp, e.hp + heal);
        }

        // Slow-immune enemies (and Juggernaut affixes) ignore slows and stuns entirely.
        const EnemyDef& def = enemyDef(e.kind);
        if (def.slowResistant || e.affix == EnemyAffix::Juggernaut) {
            e.slowFactor = 1.0f;
            e.stunTimer = 0.0f;
        }

        // ---- Titan Boss Special Abilities
        if (e.isBoss && e.hp > 0) {
            e.abilityCooldown -= dt;
            if (e.abilityCooldown <= 0) {
                if (e.kind == EnemyKind::Boss) {
                    // Colossus Titan: Earthquake Stomp!
                    e.abilityCooldown = std::max(6.0f, 10.0f - titanSurgeTier() * 1.0f);
                    pushEvent(GameEvent::TitanAbility, "EARTHQUAKE STOMP!", Vec2{e.x, e.y}, 1);
                    addFloat(Vec2{e.x, e.y}, "EARTHQUAKE STOMP!", {1.0f, 0.45f, 0.2f, 1.0f});
                    const float stompRange = 2.5f * ts;
                    for (auto& t : towers_) {
                        if (Vec2{e.x, e.y}.distanceTo(cellToWorld(t.cell)) <= stompRange) {
                            t.cooldown = std::max(t.cooldown, 2.0f);
                            addFloat(cellToWorld(t.cell), "DISRUPTED", {0.8f, 0.8f, 0.8f, 0.9f});
                        }
                    }
                } else if (e.kind == EnemyKind::BossSiege) {
                    // Ravager Titan: Berserk Cleave!
                    e.abilityCooldown = std::max(5.0f, 8.5f - titanSurgeTier() * 0.8f);
                    pushEvent(GameEvent::TitanAbility, "RAVAGE CLEAVE!", Vec2{e.x, e.y}, 2);
                    addFloat(Vec2{e.x, e.y}, "RAVAGE CLEAVE!", {1.0f, 0.2f, 0.3f, 1.0f});
                    const float cleaveRange = 2.0f * ts;
                    for (auto& t : towers_) {
                        if (Vec2{e.x, e.y}.distanceTo(cellToWorld(t.cell)) <= cleaveRange) {
                            damageTower(t.id, 20);
                            addFloat(cellToWorld(t.cell), "-20", {1.0f, 0.3f, 0.2f, 1.0f});
                        }
                    }
                }
            }
        }

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
                    const float rateMult = (e.affix == EnemyAffix::Frenzied) ? 1.35f : 1.0f;
                    e.attackCooldown = 1.0f / std::max(0.1f, def.attackRate * rateMult);
                    TitanSurgeStats surge = calcTitanSurge(waveNumber_, difficulty_);
                    const int finalDmg = std::max(1, static_cast<int>(def.damage * surge.siegeDamageMultiplier));
                    damageTower(target->id, finalDmg);
                    addFloat(targetPos, "-" + std::to_string(finalDmg),
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
    float r = def.level(t.level - 1).range * static_cast<float>(config_.tileSize);
    if (t.overdriveRemaining > 0) r *= 1.25f;
    return r;
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

        const Vec2 goalPos = cellToWorld(goal_);
        const float progress = goalPos.distanceTo(Vec2{e.x, e.y});
        float score = -progress;

        switch (t.targetPriority) {
            case TargetPriority::First:
                score = -progress + (e.isBoss ? 30.0f : 0.0f);
                break;
            case TargetPriority::Strongest:
                score = static_cast<float>(e.hp) + (e.isBoss ? 1000.0f : 0.0f);
                break;
            case TargetPriority::Weakest:
                score = -static_cast<float>(e.hp);
                break;
            case TargetPriority::Titans:
                score = (e.isBoss ? 50000.0f : 0.0f) - progress;
                break;
        }

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
        if (t.overdriveRemaining > 0) {
            t.overdriveRemaining -= dt;
            t.cooldown -= dt * 0.8f; // Cooldown ticks down much faster during Overdrive
        }
        const TowerDef& def = towerDef(t.kind);
        const TowerLevel& lv = def.level(t.level - 1);

        // ---- Aura towers (Tar Pit) damage everything in range continuously.
        if (def.mode == AttackMode::Aura) {
            if (def.kind == TowerKind::Beacon) continue;   // the beacon deals no damage
            const float r = lv.range * ts * (t.overdriveRemaining > 0 ? 1.25f : 1.0f);
            const Vec2 origin = cellToWorld(t.cell);
            for (auto& e : enemies_) {
                if (origin.distanceTo(Vec2{e.x, e.y}) > r) continue;
                if (!enemyIsTargetable(t, e)) continue;
                const int dmg = static_cast<int>(lv.auraDps * dt * (t.overdriveRemaining > 0 ? 1.5f : 1.0f));
                applyDamage(e, dmg,
                            lv.slowFactor, lv.slowDuration, 0, 0, StatusEffect::Slow);
                t.totalDamage += dmg;
                combatStats_.totalDamageDealt += dmg;
                size_t kIdx = static_cast<size_t>(t.kind);
                if (kIdx < 12) combatStats_.damageByTowerKind[kIdx] += dmg;
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
        const float fireRate = lv.fireRate * (t.overdriveRemaining > 0 ? 1.75f : 1.0f);
        t.cooldown = 1.0f / std::max(0.05f, fireRate);
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
    for (auto& t : towers_) {
        if (t.id == p.towerId) {
            t.totalDamage += p.damage;
            combatStats_.totalDamageDealt += p.damage;
            size_t kIdx = static_cast<size_t>(t.kind);
            if (kIdx < 12) combatStats_.damageByTowerKind[kIdx] += p.damage;
            break;
        }
    }

    // ---- direct damage
    if (p.chainTargets > 0 && target.poisonTimer > 0) {
        p.damage = static_cast<int>(p.damage * 1.5f);
        p.chainTargets += 2;
        combatStats_.combosTriggered++;
        addFloat(Vec2{target.x, target.y}, "SUPERCONDUCTOR!", {0.8f, 0.45f, 1.0f, 1.0f});
        pushEvent(GameEvent::ComboTriggered, "SUPERCONDUCTOR", Vec2{target.x, target.y});
    }

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

    // Shielded affix absorbs 50% damage until barrier is exhausted
    if (e.affix == EnemyAffix::Shielded && e.shieldHp > 0) {
        float absorb = std::min(static_cast<float>(dmg) * 0.5f, e.shieldHp);
        e.shieldHp -= absorb;
        dmg = std::max(1, static_cast<int>(dmg - absorb));
        addFloat(Vec2{e.x, e.y}, "SHIELD", {0.35f, 0.85f, 1.0f, 0.9f});
    }

    // ---- Elemental Synergies ----
    // 1. Thermal Shock: Burning enemy hit by Slow, or Slowed enemy hit by Burn
    if (e.burnTimer > 0 && slowFactor > 0 && slowDuration > 0) {
        const int shockDmg = static_cast<int>(amount * 0.75f) + 35;
        dmg += shockDmg;
        combatStats_.combosTriggered++;
        addFloat(Vec2{e.x, e.y}, "THERMAL SHOCK! +" + std::to_string(shockDmg), {1.0f, 0.55f, 0.15f, 1.0f});
        pushEvent(GameEvent::ComboTriggered, "THERMAL SHOCK", Vec2{e.x, e.y}, shockDmg);
        const float aoeR = 1.4f * static_cast<float>(config_.tileSize);
        for (auto& other : enemies_) {
            if (other.id != e.id && Vec2{e.x, e.y}.distanceTo(Vec2{other.x, other.y}) <= aoeR) {
                other.hp -= shockDmg / 2;
                other.hitFlash = 0.1f;
            }
        }
    } else if (e.slowTimer > 0 && effect == StatusEffect::Burn) {
        const int shockDmg = static_cast<int>(amount * 0.75f) + 35;
        dmg += shockDmg;
        combatStats_.combosTriggered++;
        addFloat(Vec2{e.x, e.y}, "THERMAL SHOCK! +" + std::to_string(shockDmg), {1.0f, 0.55f, 0.15f, 1.0f});
        pushEvent(GameEvent::ComboTriggered, "THERMAL SHOCK", Vec2{e.x, e.y}, shockDmg);
    }

    // 2. Armor Shatter: Heavy attacks shatter armored units
    if (e.armor > 0 && amount >= 50) {
        const int shatter = std::max(1, static_cast<int>(e.armor * 0.30f));
        e.armor -= shatter;
        combatStats_.combosTriggered++;
        addFloat(Vec2{e.x, e.y}, "SHATTER -" + std::to_string(shatter) + " ARMOR", {1.0f, 0.85f, 0.35f, 1.0f});
        pushEvent(GameEvent::ComboTriggered, "ARMOR SHATTER", Vec2{e.x, e.y});
    }

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
    int bounty = e.bounty;
    if (goldRushTimer_ > 0) bounty = static_cast<int>(bounty * 1.5f);
    gold_ += bounty;
    score_ += e.isBoss ? 500 : (10 + bounty * 2);
    kills_++;
    combatStats_.totalEnemiesKilled++;
    combatStats_.totalGoldEarned += bounty;
    if (e.isBoss) {
        combatStats_.titansDefeated++;
    }
    addFloat(Vec2{e.x, e.y}, "+" + std::to_string(bounty),
             {1.0f, 0.85f, 0.3f, 1.0f});
    pushEvent(GameEvent::EnemyKilled, enemyDef(e.kind).name, Vec2{e.x, e.y}, bounty);

    // Powerup orb drop
    if (e.isBoss) {
        PowerupKind drop = (e.id % 2 == 0) ? PowerupKind::OverchargeAll : PowerupKind::CryoFreeze;
        if (lives_ < config_.startingLives && (e.id % 3 == 0)) drop = PowerupKind::CoreLife;
        spawnPowerup(Vec2{e.x, e.y}, drop);
    } else if (e.affix != EnemyAffix::None) {
        PowerupKind drop = (e.id % 2 == 0) ? PowerupKind::GoldCache : PowerupKind::OverchargeAll;
        spawnPowerup(Vec2{e.x, e.y}, drop);
    } else if ((e.id % 6) == 0) {
        spawnPowerup(Vec2{e.x, e.y}, PowerupKind::GoldCache);
    }

    // Vengeful Affix death retribution
    if (e.affix == EnemyAffix::Vengeful) {
        addFloat(Vec2{e.x, e.y}, "VENGEANCE!", {1.0f, 0.25f, 0.25f, 1.0f});
        const float radius = 1.6f * static_cast<float>(config_.tileSize);
        for (auto& t : towers_) {
            if (Vec2{e.x, e.y}.distanceTo(cellToWorld(t.cell)) <= radius) {
                damageTower(t.id, 24);
                addFloat(cellToWorld(t.cell), "-24", {1.0f, 0.35f, 0.2f, 1.0f});
            }
        }
        for (auto& other : enemies_) {
            if (other.id != e.id && Vec2{e.x, e.y}.distanceTo(Vec2{other.x, other.y}) <= radius) {
                other.speed *= 1.25f;
                addFloat(Vec2{other.x, other.y}, "ENRAGED", {1.0f, 0.6f, 0.15f, 1.0f});
            }
        }
    }
}

}  // namespace tf
