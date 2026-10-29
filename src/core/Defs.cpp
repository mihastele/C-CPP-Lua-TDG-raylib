#include "core/Defs.h"

namespace tf {
namespace {

// Helper to build a tower's three levels without repeating the struct syntax.
TowerLevel L(int cost, int dmg, float range, float rate,
             float splash = 0, int chain = 0,
             float slow = 0, float slowDur = 0,
             float statusDur = 0, float statusDps = 0,
             int pierce = 0, float auraDps = 0) {
    TowerLevel l;
    l.cost = cost;
    l.damage = dmg;
    l.range = range;
    l.fireRate = rate;
    l.splashRadius = splash;
    l.chainTargets = chain;
    l.slowFactor = slow;
    l.slowDuration = slowDur;
    l.statusDuration = statusDur;
    l.statusDps = statusDps;
    l.pierceCount = pierce;
    l.auraDps = auraDps;
    return l;
}

std::vector<TowerDef> buildTowers() {
    std::vector<TowerDef> t;

    // --- Arrow: the reliable opener. Cheap, fast, single target.
    {
        TowerDef d;
        d.kind = TowerKind::Arrow;
        d.name = "Archer Post";
        d.description = "Rapid single-target arrows. Cheap, reliable, always useful.";
        d.mode = AttackMode::Single;
        d.style = ProjectileStyle::Arrow;
        d.projectileSpeed = 16.0f;
        d.levels = {
            L(60, 9, 3.2f, 1.8f),
            L(45, 15, 3.6f, 2.1f),
            L(90, 26, 4.0f, 2.5f),
        };
        d.upgradeNote = "Sharper arrows, longer reach.";
        t.push_back(d);
    }

    // --- Cannon: slow, splash. The crowd-control workhorse.
    {
        TowerDef d;
        d.kind = TowerKind::Cannon;
        d.name = "Mortar Pit";
        d.description = "Lobs shells that splash. Slow to reload, devastating to packs.";
        d.mode = AttackMode::Splash;
        d.style = ProjectileStyle::Cannonball;
        d.projectileSpeed = 9.0f;
        d.levels = {
            L(110, 26, 2.8f, 0.65f, 1.2f),
            L(95, 46, 3.1f, 0.75f, 1.5f),
            L(180, 78, 3.5f, 0.85f, 1.9f),
        };
        d.upgradeNote = "Bigger shell, wider blast.";
        t.push_back(d);
    }

    // --- Frost: slows. Never kills fast, but makes everything else work.
    {
        TowerDef d;
        d.kind = TowerKind::Frost;
        d.name = "Frost Spire";
        d.description = "Chills enemies, slowing them so the rest of your towers get more shots.";
        d.mode = AttackMode::Single;
        d.style = ProjectileStyle::FrostShard;
        d.projectileSpeed = 13.0f;
        d.targetsAir = true;
        d.levels = {
            L(90, 6, 2.9f, 1.1f, 0, 0, 0.40f, 2.0f),
            L(70, 9, 3.2f, 1.2f, 0, 0, 0.50f, 2.5f),
            L(140, 14, 3.6f, 1.35f, 0, 0, 0.62f, 3.2f),
        };
        d.upgradeNote = "Colder: slower and longer-lasting chill.";
        t.push_back(d);
    }

    // --- Tesla: chains. Rewards tight clusters.
    {
        TowerDef d;
        d.kind = TowerKind::Tesla;
        d.name = "Tesla Coil";
        d.description = "Lightning arcs to nearby enemies after hitting the first.";
        d.mode = AttackMode::Chain;
        d.style = ProjectileStyle::Lightning;
        d.projectileSpeed = 30.0f;
        d.turnRate = 12.0f;
        d.levels = {
            L(130, 20, 2.6f, 1.0f, 0, 2),
            L(110, 33, 2.8f, 1.15f, 0, 3),
            L(210, 52, 3.1f, 1.3f, 0, 5),
        };
        d.upgradeNote = "More jumps, more damage per arc.";
        t.push_back(d);
    }

    // --- Sniper: very long range, very slow, huge damage. The boss answer.
    {
        TowerDef d;
        d.kind = TowerKind::Sniper;
        d.name = "Longshot";
        d.description = "Enormous range and damage, but it reloads slowly. Line it up on bosses.";
        d.mode = AttackMode::Single;
        d.style = ProjectileStyle::Bullet;
        d.projectileSpeed = 40.0f;
        d.turnRate = 5.0f;
        d.levels = {
            L(150, 95, 7.5f, 0.42f),
            L(130, 165, 8.2f, 0.48f),
            L(260, 280, 9.0f, 0.55f),
        };
        d.upgradeNote = "More range and a heavier round.";
        t.push_back(d);
    }

    // --- Poison: damage over time. Best against high-HP single targets.
    {
        TowerDef d;
        d.kind = TowerKind::Poison;
        d.name = "Venom Spout";
        d.description = "Poisoned bolts that keep hurting after they land.";
        d.mode = AttackMode::Single;
        d.style = ProjectileStyle::PoisonBottle;
        d.projectileSpeed = 11.0f;
        d.levels = {
            L(120, 12, 3.0f, 0.9f, 0, 0, 0, 0, 4.0f, 6.0f),
            L(100, 18, 3.3f, 1.0f, 0, 0, 0, 0, 5.5f, 10.0f),
            L(190, 28, 3.7f, 1.15f, 0, 0, 0, 0, 7.0f, 16.0f),
        };
        d.upgradeNote = "Toxicity and duration both up.";
        t.push_back(d);
    }

    // --- Flame: short range cone, hits everything in front of it.
    {
        TowerDef d;
        d.kind = TowerKind::Flame;
        d.name = "Ember Vent";
        d.description = "A short, fierce cone of fire that scorches everything in front of it.";
        d.mode = AttackMode::Single;
        d.style = ProjectileStyle::Fireball;
        d.projectileSpeed = 14.0f;
        d.turnRate = 3.0f;
        d.levels = {
            L(100, 8, 2.0f, 2.6f, 0, 0, 0, 0, 1.6f, 4.0f),
            L(85, 13, 2.2f, 2.9f, 0, 0, 0, 0, 2.0f, 7.0f),
            L(160, 21, 2.5f, 3.3f, 0, 0, 0, 0, 2.6f, 11.0f),
        };
        d.upgradeNote = "Wider reach and hotter burn.";
        t.push_back(d);
    }

    // --- Prism: splits its shot into multiple beams.
    {
        TowerDef d;
        d.kind = TowerKind::Prism;
        d.name = "Prism Spire";
        d.description = "Splits one shot into several beams, hitting a whole group at once.";
        d.mode = AttackMode::Pierce;
        d.style = ProjectileStyle::Prism;
        d.projectileSpeed = 22.0f;
        d.levels = {
            L(140, 11, 3.1f, 1.3f, 0, 0, 0, 0, 0, 0, 3),
            L(120, 17, 3.4f, 1.45f, 0, 0, 0, 0, 0, 0, 5),
            L(230, 27, 3.8f, 1.6f, 0, 0, 0, 0, 0, 0, 8),
        };
        d.upgradeNote = "More beams per shot.";
        t.push_back(d);
    }

    // --- Mortar: long range arcing shell with a big splash.
    {
        TowerDef d;
        d.kind = TowerKind::Mortar;
        d.name = "Sky Mortar";
        d.description = "Lobs a shell over everything and brings it down on the far side.";
        d.mode = AttackMode::Splash;
        d.style = ProjectileStyle::MortarShell;
        d.projectileSpeed = 7.5f;
        d.turnRate = 2.5f;
        d.levels = {
            L(160, 55, 6.0f, 0.4f, 1.8f),
            L(140, 92, 6.6f, 0.46f, 2.1f),
            L(270, 155, 7.2f, 0.52f, 2.6f),
        };
        d.upgradeNote = "Longer reach, bigger shell.";
        t.push_back(d);
    }

    // --- Ballista: pierces in a straight line, best against a column.
    {
        TowerDef d;
        d.kind = TowerKind::Ballista;
        d.name = "Rail Ballista";
        d.description = "A heavy bolt that punches clean through a whole line of enemies.";
        d.mode = AttackMode::Pierce;
        d.style = ProjectileStyle::Pierce;
        d.projectileSpeed = 26.0f;
        d.levels = {
            L(135, 38, 4.6f, 0.75f, 0, 0, 0, 0, 0, 0, 4),
            L(115, 62, 5.0f, 0.85f, 0, 0, 0, 0, 0, 0, 7),
            L(220, 105, 5.5f, 0.95f, 0, 0, 0, 0, 0, 0, 12),
        };
        d.upgradeNote = "Heavier bolt, pierces more.";
        t.push_back(d);
    }

    // --- Tarpit: an aura that slows and burns everything standing in it.
    {
        TowerDef d;
        d.kind = TowerKind::Tarpit;
        d.name = "Tar Pit";
        d.description = "A pool that drags enemies down and keeps chewing on them.";
        d.mode = AttackMode::Aura;
        d.style = ProjectileStyle::Aura;
        d.levels = {
            L(125, 0, 1.9f, 0, 0, 0, 0.45f, 0.6f, 0, 0, 0, 9.0f),
            L(105, 0, 2.2f, 0, 0, 0, 0.55f, 0.8f, 0, 0, 0, 15.0f),
            L(200, 0, 2.6f, 0, 0, 0, 0.68f, 1.0f, 0, 0, 0, 24.0f),
        };
        d.upgradeNote = "Wider pool, stickier, more corrosive.";
        t.push_back(d);
    }

    // --- Beacon: buffs the damage of nearby towers. Support, not damage.
    {
        TowerDef d;
        d.kind = TowerKind::Beacon;
        d.name = "War Beacon";
        d.description = "No damage of its own, but every tower beside it hits harder.";
        d.mode = AttackMode::Aura;
        d.style = ProjectileStyle::Aura;
        d.levels = {
            L(140, 0, 3.0f, 0, 0, 0, 0, 0, 0, 0, 0, 0),
            L(120, 0, 3.4f, 0, 0, 0, 0, 0, 0, 0, 0, 0),
            L(240, 0, 3.9f, 0, 0, 0, 0, 0, 0, 0, 0, 0),
        };
        d.upgradeNote = "Wider radius of influence.";
        t.push_back(d);
    }

    // --- Ballista Mk II: high single-target burst.
    {
        TowerDef d;
        d.kind = TowerKind::Ballista;
        d.name = "Siege Ballista";
        d.description = "Slow, colossal bolts. Put it where a boss has to walk.";
        d.mode = AttackMode::Single;
        d.style = ProjectileStyle::Cannonball;
        d.projectileSpeed = 15.0f;
        d.turnRate = 3.5f;
        d.levels = {
            L(145, 72, 5.2f, 0.5f),
            L(125, 118, 5.6f, 0.56f),
            L(250, 195, 6.1f, 0.62f),
        };
        d.upgradeNote = "Bigger bolt, more reach.";
        t.push_back(d);
    }

    return t;
}

std::vector<EnemyDef> buildEnemies() {
    std::vector<EnemyDef> e;

    // ---- walkers: they follow the path and try to reach the exit
    {
        EnemyDef d;
        d.kind = EnemyKind::Grunt; d.name = "Grunt";
        d.archetype = EnemyArchetype::Walker;
        d.hp = 30; d.speed = 1.7f; d.bounty = 8; d.scale = 1.0f;
        d.color = "red"; d.shape = "blob";
        e.push_back(d);
    }
    {
        EnemyDef d;
        d.kind = EnemyKind::Runner; d.name = "Runner";
        d.archetype = EnemyArchetype::Walker;
        d.hp = 18; d.speed = 3.4f; d.bounty = 9; d.scale = 0.85f;
        d.color = "orange"; d.shape = "sprite";
        e.push_back(d);
    }
    {
        EnemyDef d;
        d.kind = EnemyKind::Brute; d.name = "Brute";
        d.archetype = EnemyArchetype::Walker;
        d.hp = 110; d.speed = 1.1f; d.bounty = 20; d.armor = 10; d.scale = 1.25f;
        d.color = "brown"; d.shape = "blob";
        e.push_back(d);
    }
    {
        EnemyDef d;
        d.kind = EnemyKind::Crawler; d.name = "Crawler";
        d.archetype = EnemyArchetype::Walker;
        d.hp = 190; d.speed = 0.85f; d.bounty = 26; d.armor = 25; d.scale = 1.35f;
        d.color = "grey"; d.shape = "bug";
        d.slowResistant = true;
        e.push_back(d);
    }
    {
        EnemyDef d;
        d.kind = EnemyKind::Wraith; d.name = "Wraith";
        d.archetype = EnemyArchetype::Walker;
        d.hp = 45; d.speed = 2.5f; d.bounty = 17; d.scale = 1.0f;
        d.color = "violet"; d.shape = "ghost";
        d.slowResistant = true;   // ignores slows
        e.push_back(d);
    }

    // ---- flyers: they ignore the path and the towers entirely
    {
        EnemyDef d;
        d.kind = EnemyKind::Flyer; d.name = "Wasp";
        d.archetype = EnemyArchetype::Flyer;
        d.hp = 26; d.speed = 2.6f; d.bounty = 13; d.scale = 0.9f;
        d.color = "yellow"; d.shape = "wing";
        e.push_back(d);
    }

    // ---- attackers: they ignore the path and go for your towers
    {
        EnemyDef d;
        d.kind = EnemyKind::Sapper; d.name = "Sapper";
        d.archetype = EnemyArchetype::Attacker;
        d.hp = 55; d.speed = 1.9f; d.bounty = 16; d.scale = 1.05f;
        d.damage = 7; d.attackRange = 0.9f; d.attackRate = 0.7f;
        d.color = "green"; d.shape = "hex";
        e.push_back(d);
    }
    {
        EnemyDef d;
        d.kind = EnemyKind::Bruiser; d.name = "Bruiser";
        d.archetype = EnemyArchetype::Attacker;
        d.hp = 150; d.speed = 1.35f; d.bounty = 30; d.scale = 1.3f; d.armor = 15;
        d.damage = 15; d.attackRange = 1.0f; d.attackRate = 0.65f;
        d.color = "darkgreen"; d.shape = "hex";
        e.push_back(d);
    }

    // ---- bosses
    {
        EnemyDef d;
        d.kind = EnemyKind::Boss; d.name = "Colossus";
        d.archetype = EnemyArchetype::Walker;
        d.hp = 950; d.speed = 0.95f; d.bounty = 260; d.armor = 30; d.scale = 2.4f;
        d.color = "purple"; d.shape = "monster";
        d.isBoss = true; d.slowResistant = true;
        e.push_back(d);
    }
    {
        EnemyDef d;
        d.kind = EnemyKind::BossSiege; d.name = "Ravager";
        d.archetype = EnemyArchetype::Attacker;
        d.hp = 1400; d.speed = 1.05f; d.bounty = 320; d.armor = 35; d.scale = 2.6f;
        d.damage = 42; d.attackRange = 1.4f; d.attackRate = 0.55f;
        d.color = "crimson"; d.shape = "monster";
        d.isBoss = true; d.slowResistant = true;
        e.push_back(d);
    }

    return e;
}

}  // namespace

const std::vector<TowerDef>& allTowerDefs() {
    static const std::vector<TowerDef> defs = buildTowers();
    return defs;
}

const std::vector<EnemyDef>& allEnemyDefs() {
    static const std::vector<EnemyDef> defs = buildEnemies();
    return defs;
}

const TowerDef& towerDef(TowerKind kind) {
    const auto& all = allTowerDefs();
    for (const auto& t : all) if (t.kind == kind) return t;
    return all.front();
}

const EnemyDef& enemyDef(EnemyKind kind) {
    const auto& all = allEnemyDefs();
    for (const auto& e : all) if (e.kind == kind) return e;
    return all.front();
}

int towerUpgradeCount(TowerKind kind) {
    return towerDef(kind).maxLevel();
}

}  // namespace tf
