// towerfall — procedural audio engine.
//
// Generates all sound effects dynamically at runtime in memory as 16-bit PCM waves.
// Zero external assets, zero files loaded, pure offline mathematical waveforms.

#pragma once

#include <cstdint>
#include <string>

namespace tf {

enum class SoundId : uint8_t {
    Shoot = 0,
    Laser,
    Hit,
    Explosion,
    Build,
    Upgrade,
    Sell,
    Repair,
    Overdrive,
    TitanRoar,
    SurgeHorn,
    Meteor,
    CryoFreeze,
    PowerupCollect,
    ComboTrigger,
    Victory,
    GameOver,
    Count
};

class AudioManager {
public:
    AudioManager();
    ~AudioManager();

    void init();
    void shutdown();
    void play(SoundId id, float pitch = 1.0f, float volumeScale = 1.0f);

    void toggleMute();
    bool isMuted() const { return muted_; }
    void setVolume(float v);
    float volume() const { return volume_; }

private:
    void generateSounds();

    bool ready_ = false;
    bool muted_ = false;
    float volume_ = 0.65f;
    void* sounds_[static_cast<size_t>(SoundId::Count)] = {nullptr};
};

}  // namespace tf
