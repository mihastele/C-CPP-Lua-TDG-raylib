// towerfall — wave definitions.
//
// Waves are described declaratively so the pacing can be tuned without touching
// the simulation. Each wave lists groups of enemies; the simulator expands them
// into a spawn schedule.

#pragma once

#include <string>
#include <vector>

#include "core/Types.h"

namespace tf {

struct WaveGroup {
    EnemyKind kind = EnemyKind::Grunt;
    int count = 1;
    float interval = 0.6f;      // seconds between spawns within the group
    float delay = 0.0f;         // seconds after the wave starts
};

struct WaveDef {
    int number = 1;
    std::string name;
    std::vector<WaveGroup> groups;
    int reward = 0;             // bonus gold for clearing the wave
    float prepBonus = 0;        // extra prep time granted before the wave
    bool boss = false;
};

const std::vector<WaveDef>& allWaves();

// The wave shown in the "next wave" preview.
const WaveDef& waveDef(int number);

// Builds the wave list for a level difficulty: "easy", "normal" or "hard".
std::vector<WaveDef> buildWaves(const std::string& difficulty, int waveCount = 15);

}  // namespace tf
