#include "core/Waves.h"

#include <cmath>

namespace tf {
namespace {

// Difficulty multipliers. "easy" is a gentle introduction, "hard" is genuinely hard.
struct Difficulty {
    float hpScale;
    float speedScale;
    int startGold;
    int startLives;
    float waveDelay;
};

Difficulty forDifficulty(const std::string& d) {
    if (d == "easy")   return {0.75f, 0.92f, 320, 30, 15.0f};
    if (d == "hard")   return {1.45f, 1.12f, 220, 15, 10.0f};
    return {1.0f, 1.0f, 260, 20, 12.0f};   // normal
}

// Which enemy kinds are unlocked by which wave number.
bool unlocked(EnemyKind k, int wave) {
    switch (k) {
        case EnemyKind::Grunt:    return wave >= 1;
        case EnemyKind::Runner:   return wave >= 3;
        case EnemyKind::Sapper:   return wave >= 5;
        case EnemyKind::Flyer:    return wave >= 6;
        case EnemyKind::Brute:    return wave >= 7;
        case EnemyKind::Wraith:   return wave >= 9;
        case EnemyKind::Bruiser:  return wave >= 11;
        case EnemyKind::Crawler:  return wave >= 12;
        default: return true;
    }
}

}  // namespace

WaveDef generateWave(int n, const std::string& difficulty) {
    WaveDef w;
    w.number = n;

    // Every fifth wave is a boss / titan wave.
    w.boss = (n % 5 == 0);

    const int tier = (difficulty == "easy") ? 0 : titanSurgeTier(n);

    // Name the wave so the player has something to react to.
    if (w.boss) {
        if (n == 5) w.name = "The First Titan";
        else if (n == 10) w.name = "Twin Tyrants";
        else if (n == 15) w.name = "Grand Apocalypse";
        else if (n == 20) w.name = "Titan Legion";
        else w.name = "Titan Dreadnoughts";
    } else if (n > 5 && (n - 5) % 4 == 0) {
        w.name = titanSurgeName(tier);
    } else if (n == 1) {
        w.name = "First Contact";
    } else if (n == 3) {
        w.name = "They Brought Friends";
    } else if (n == 7) {
        w.name = "Air Support";
    } else if (n == 11) {
        w.name = "The Heavy Wave";
    } else {
        w.name = "Wave " + std::to_string(n);
    }

    // ---- grunt pressure, always present and growing
    WaveGroup g;
    g.kind = EnemyKind::Grunt;
    g.count = 4 + static_cast<int>(std::floor(n * 1.25f)) + (tier > 0 ? tier * 3 : 0);
    g.interval = std::max(0.25f, 0.55f - tier * 0.05f);
    g.delay = 0.0f;
    w.groups.push_back(g);

    // ---- runners from wave 3
    if (unlocked(EnemyKind::Runner, n)) {
        WaveGroup r;
        r.kind = EnemyKind::Runner;
        r.count = 2 + static_cast<int>(std::floor(n * 0.7f)) + (tier > 0 ? tier * 2 : 0);
        r.interval = std::max(0.18f, 0.35f - tier * 0.03f);
        r.delay = 2.0f;
        w.groups.push_back(r);
    }

    // ---- sappers from wave 5: the first enemy that attacks towers
    if (unlocked(EnemyKind::Sapper, n)) {
        WaveGroup s;
        s.kind = EnemyKind::Sapper;
        s.count = 1 + static_cast<int>(std::floor((n - 5) * 0.55f)) + (tier > 0 ? tier * 2 : 0);
        s.interval = std::max(0.6f, 1.2f - tier * 0.1f);
        s.delay = 4.0f;
        w.groups.push_back(s);
    }

    // ---- flyers from wave 6: they ignore the path, so mazing is useless on them
    if (unlocked(EnemyKind::Flyer, n)) {
        WaveGroup f;
        f.kind = EnemyKind::Flyer;
        f.count = 1 + static_cast<int>(std::floor((n - 6) * 0.5f)) + (tier > 0 ? tier : 0);
        f.interval = std::max(0.45f, 0.9f - tier * 0.08f);
        f.delay = 6.0f;
        w.groups.push_back(f);
    }

    // ---- brutes from wave 7
    if (unlocked(EnemyKind::Brute, n)) {
        WaveGroup b;
        b.kind = EnemyKind::Brute;
        b.count = 1 + static_cast<int>(std::floor((n - 7) * 0.45f)) + (tier > 0 ? tier : 0);
        b.interval = 1.6f;
        b.delay = 8.0f;
        w.groups.push_back(b);
    }

    // ---- wraiths from wave 9 (slow-immune)
    if (unlocked(EnemyKind::Wraith, n)) {
        WaveGroup wr;
        wr.kind = EnemyKind::Wraith;
        wr.count = 1 + static_cast<int>(std::floor((n - 9) * 0.4f)) + (tier > 0 ? tier : 0);
        wr.interval = 1.0f;
        wr.delay = 10.0f;
        w.groups.push_back(wr);
    }

    // ---- bruisers from wave 11 (tanky tower-gnawers)
    if (unlocked(EnemyKind::Bruiser, n)) {
        WaveGroup bu;
        bu.kind = EnemyKind::Bruiser;
        bu.count = 1 + static_cast<int>(std::floor((n - 11) * 0.35f)) + (tier > 0 ? tier : 0);
        bu.interval = 2.0f;
        bu.delay = 12.0f;
        w.groups.push_back(bu);
    }

    // ---- crawlers from wave 12
    if (unlocked(EnemyKind::Crawler, n)) {
        WaveGroup c;
        c.kind = EnemyKind::Crawler;
        c.count = 1 + static_cast<int>(std::floor((n - 12) * 0.3f)) + (tier > 0 ? tier : 0);
        c.interval = 2.4f;
        c.delay = 14.0f;
        w.groups.push_back(c);
    }

    // ---- the boss / titan spawns
    if (w.boss) {
        WaveGroup boss;
        // Every tenth wave sends both bosses.
        boss.kind = (n % 10 == 0) ? EnemyKind::BossSiege : EnemyKind::Boss;
        boss.count = (n % 10 == 0) ? 2 : 1;
        boss.interval = 3.0f;
        boss.delay = 5.0f;
        w.groups.push_back(boss);

        if (n % 10 == 0) {
            WaveGroup boss2;
            boss2.kind = EnemyKind::Boss;
            boss2.count = 1 + (tier > 1 ? (tier - 1) : 0);
            boss2.interval = 3.0f;
            boss2.delay = 9.0f;
            w.groups.push_back(boss2);
        } else if (n >= 15) {
            WaveGroup boss2;
            boss2.kind = EnemyKind::BossSiege;
            boss2.count = tier;
            boss2.interval = 4.0f;
            boss2.delay = 10.0f;
            w.groups.push_back(boss2);
        }
    }

    // ---- rewards
    w.reward = 45 + n * 14 + tier * 25;
    w.prepBonus = (w.boss ? 6.0f : 0.0f) + (tier > 0 ? 2.0f : 0.0f);

    return w;
}

std::vector<WaveDef> buildWaves(const std::string& difficulty, int waveCount) {
    std::vector<WaveDef> waves;
    waves.reserve(static_cast<size_t>(waveCount));

    for (int n = 1; n <= waveCount; ++n) {
        waves.push_back(generateWave(n, difficulty));
    }
    return waves;
}

const std::vector<WaveDef>& allWaves() {
    static const std::vector<WaveDef> waves = buildWaves("normal", 15);
    return waves;
}

const WaveDef& waveDef(int number) {
    const auto& all = allWaves();
    if (number >= 1 && number <= static_cast<int>(all.size())) {
        return all[static_cast<size_t>(number - 1)];
    }
    // For endless mode waves beyond 15, cache dynamically
    static std::vector<WaveDef> endlessCache;
    for (const auto& w : endlessCache) {
        if (w.number == number) return w;
    }
    endlessCache.push_back(generateWave(number, "normal"));
    return endlessCache.back();
}

}  // namespace tf
