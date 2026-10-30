// towerfall — procedural audio synthesis implementation.

#include "render/Audio.h"

#include <cmath>
#include <cstdlib>
#include <vector>

#include "raylib.h"

namespace tf {
namespace {

constexpr int kSampleRate = 22050;
constexpr float kPi = 3.14159265358979323846f;

Sound createProceduralSound(const std::vector<int16_t>& samples) {
    Wave wave;
    wave.frameCount = static_cast<unsigned int>(samples.size());
    wave.sampleRate = kSampleRate;
    wave.sampleSize = 16;
    wave.channels = 1;
    wave.data = const_cast<void*>(static_cast<const void*>(samples.data()));

    Sound s = LoadSoundFromWave(wave);
    return s;
}

std::vector<int16_t> genShoot() {
    const float duration = 0.12f;
    const size_t totalSamples = static_cast<size_t>(kSampleRate * duration);
    std::vector<int16_t> out(totalSamples);
    float phase = 0;
    for (size_t i = 0; i < totalSamples; ++i) {
        float t = static_cast<float>(i) / kSampleRate;
        float progress = t / duration;
        // Pitch sweep: 850 Hz -> 220 Hz
        float freq = 850.0f - 630.0f * progress;
        phase += 2.0f * kPi * freq / kSampleRate;
        float amp = (1.0f - progress) * 0.7f;
        float s = std::sin(phase) * amp;
        out[i] = static_cast<int16_t>(s * 32000.0f);
    }
    return out;
}

std::vector<int16_t> genLaser() {
    const float duration = 0.15f;
    const size_t totalSamples = static_cast<size_t>(kSampleRate * duration);
    std::vector<int16_t> out(totalSamples);
    float phase = 0;
    for (size_t i = 0; i < totalSamples; ++i) {
        float t = static_cast<float>(i) / kSampleRate;
        float progress = t / duration;
        // Pitch sweep: 1400 Hz -> 400 Hz (fast sci-fi laser)
        float freq = 1400.0f - 1000.0f * std::sqrt(progress);
        phase += 2.0f * kPi * freq / kSampleRate;
        float amp = (1.0f - progress) * 0.65f;
        // Blend sine with soft triangle
        float sineVal = std::sin(phase);
        float triVal = std::asin(sineVal) * (2.0f / kPi);
        float s = (sineVal * 0.6f + triVal * 0.4f) * amp;
        out[i] = static_cast<int16_t>(s * 32000.0f);
    }
    return out;
}

std::vector<int16_t> genHit() {
    const float duration = 0.05f;
    const size_t totalSamples = static_cast<size_t>(kSampleRate * duration);
    std::vector<int16_t> out(totalSamples);
    float phase = 0;
    for (size_t i = 0; i < totalSamples; ++i) {
        float t = static_cast<float>(i) / kSampleRate;
        float progress = t / duration;
        float freq = 320.0f - 180.0f * progress;
        phase += 2.0f * kPi * freq / kSampleRate;
        float noise = (static_cast<float>(std::rand()) / RAND_MAX * 2.0f - 1.0f);
        float env = (1.0f - progress) * (1.0f - progress);
        float s = (std::sin(phase) * 0.6f + noise * 0.4f) * env * 0.8f;
        out[i] = static_cast<int16_t>(s * 32000.0f);
    }
    return out;
}

std::vector<int16_t> genExplosion() {
    const float duration = 0.50f;
    const size_t totalSamples = static_cast<size_t>(kSampleRate * duration);
    std::vector<int16_t> out(totalSamples);
    float phase = 0;
    float lpf = 0;
    for (size_t i = 0; i < totalSamples; ++i) {
        float t = static_cast<float>(i) / kSampleRate;
        float progress = t / duration;
        // Deep sub-bass drop: 75 Hz -> 28 Hz
        float freq = 75.0f - 47.0f * progress;
        phase += 2.0f * kPi * freq / kSampleRate;
        float noise = (static_cast<float>(std::rand()) / RAND_MAX * 2.0f - 1.0f);
        // Simple low-pass filter on noise
        lpf += (noise - lpf) * (0.35f - 0.25f * progress);
        float env = std::exp(-4.5f * progress);
        float s = (std::sin(phase) * 0.5f + lpf * 0.5f) * env * 0.85f;
        out[i] = static_cast<int16_t>(s * 32000.0f);
    }
    return out;
}

std::vector<int16_t> genBuild() {
    const float duration = 0.22f;
    const size_t totalSamples = static_cast<size_t>(kSampleRate * duration);
    std::vector<int16_t> out(totalSamples);
    float phase1 = 0, phase2 = 0;
    for (size_t i = 0; i < totalSamples; ++i) {
        float t = static_cast<float>(i) / kSampleRate;
        float progress = t / duration;
        // Ascending harmonic fifth: 440 Hz -> 660 Hz
        float freq = (progress < 0.5f) ? 440.0f : 660.0f;
        float stepProgress = (progress < 0.5f) ? (progress * 2.0f) : ((progress - 0.5f) * 2.0f);
        phase1 += 2.0f * kPi * freq / kSampleRate;
        phase2 += 2.0f * kPi * (freq * 1.5f) / kSampleRate;
        float env = (1.0f - stepProgress) * 0.6f;
        float s = (std::sin(phase1) * 0.7f + std::sin(phase2) * 0.3f) * env;
        out[i] = static_cast<int16_t>(s * 32000.0f);
    }
    return out;
}

std::vector<int16_t> genUpgrade() {
    const float duration = 0.35f;
    const size_t totalSamples = static_cast<size_t>(kSampleRate * duration);
    std::vector<int16_t> out(totalSamples);
    const float notes[3] = {523.25f, 659.25f, 783.99f}; // C5, E5, G5
    float phase = 0;
    for (size_t i = 0; i < totalSamples; ++i) {
        float t = static_cast<float>(i) / kSampleRate;
        int noteIdx = std::min(2, static_cast<int>(t / (duration / 3.0f)));
        float localT = std::fmod(t, duration / 3.0f) / (duration / 3.0f);
        float freq = notes[noteIdx];
        phase += 2.0f * kPi * freq / kSampleRate;
        float env = (1.0f - localT) * 0.65f;
        float s = std::sin(phase) * env;
        out[i] = static_cast<int16_t>(s * 32000.0f);
    }
    return out;
}

std::vector<int16_t> genSell() {
    const float duration = 0.18f;
    const size_t totalSamples = static_cast<size_t>(kSampleRate * duration);
    std::vector<int16_t> out(totalSamples);
    float phase1 = 0, phase2 = 0;
    for (size_t i = 0; i < totalSamples; ++i) {
        float t = static_cast<float>(i) / kSampleRate;
        float progress = t / duration;
        phase1 += 2.0f * kPi * 987.77f / kSampleRate;  // B5
        phase2 += 2.0f * kPi * 1318.51f / kSampleRate; // E6
        float env = (1.0f - progress) * 0.55f;
        float s = (std::sin(phase1) * 0.5f + std::sin(phase2) * 0.5f) * env;
        out[i] = static_cast<int16_t>(s * 32000.0f);
    }
    return out;
}

std::vector<int16_t> genRepair() {
    const float duration = 0.28f;
    const size_t totalSamples = static_cast<size_t>(kSampleRate * duration);
    std::vector<int16_t> out(totalSamples);
    float phase = 0;
    for (size_t i = 0; i < totalSamples; ++i) {
        float t = static_cast<float>(i) / kSampleRate;
        float progress = t / duration;
        float freq = 380.0f + 240.0f * std::sin(progress * kPi);
        phase += 2.0f * kPi * freq / kSampleRate;
        float env = (1.0f - progress) * 0.6f;
        float s = std::sin(phase) * env;
        out[i] = static_cast<int16_t>(s * 32000.0f);
    }
    return out;
}

std::vector<int16_t> genOverdrive() {
    const float duration = 0.60f;
    const size_t totalSamples = static_cast<size_t>(kSampleRate * duration);
    std::vector<int16_t> out(totalSamples);
    float phase = 0;
    for (size_t i = 0; i < totalSamples; ++i) {
        float t = static_cast<float>(i) / kSampleRate;
        float progress = t / duration;
        // Rising sci-fi charging sweep: 220 Hz -> 880 Hz with vibrato
        float vib = 1.0f + 0.08f * std::sin(progress * 40.0f);
        float freq = (220.0f + 660.0f * (progress * progress)) * vib;
        phase += 2.0f * kPi * freq / kSampleRate;
        float env = (progress < 0.8f) ? (progress / 0.8f) : ((1.0f - progress) / 0.2f);
        float s = std::sin(phase) * env * 0.75f;
        out[i] = static_cast<int16_t>(s * 32000.0f);
    }
    return out;
}

std::vector<int16_t> genTitanRoar() {
    const float duration = 1.1f;
    const size_t totalSamples = static_cast<size_t>(kSampleRate * duration);
    std::vector<int16_t> out(totalSamples);
    float phase1 = 0, phase2 = 0;
    for (size_t i = 0; i < totalSamples; ++i) {
        float t = static_cast<float>(i) / kSampleRate;
        float progress = t / duration;
        // Low menacing roar: 110 Hz -> 35 Hz with detuned saw
        float freq = 110.0f - 75.0f * std::sqrt(progress);
        phase1 += 2.0f * kPi * freq / kSampleRate;
        phase2 += 2.0f * kPi * (freq * 1.03f) / kSampleRate;
        float saw1 = std::fmod(phase1 / (2.0f * kPi), 1.0f) * 2.0f - 1.0f;
        float saw2 = std::fmod(phase2 / (2.0f * kPi), 1.0f) * 2.0f - 1.0f;
        float tremolo = 0.75f + 0.25f * std::sin(t * 30.0f);
        float env = (1.0f - progress) * tremolo * 0.85f;
        float s = (saw1 * 0.5f + saw2 * 0.5f) * env;
        out[i] = static_cast<int16_t>(s * 32000.0f);
    }
    return out;
}

std::vector<int16_t> genSurgeHorn() {
    const float duration = 1.0f;
    const size_t totalSamples = static_cast<size_t>(kSampleRate * duration);
    std::vector<int16_t> out(totalSamples);
    float phase1 = 0, phase2 = 0;
    for (size_t i = 0; i < totalSamples; ++i) {
        float t = static_cast<float>(i) / kSampleRate;
        float progress = t / duration;
        // Dissonant brass war horn: 130 Hz & 195 Hz
        phase1 += 2.0f * kPi * 130.81f / kSampleRate; // C3
        phase2 += 2.0f * kPi * 196.00f / kSampleRate; // G3
        float saw1 = std::sin(phase1);
        float saw2 = std::sin(phase2);
        // Brass envelope: attack 0.15s, then sustain and decay
        float env = 0;
        if (progress < 0.15f) env = progress / 0.15f;
        else env = (1.0f - (progress - 0.15f) / 0.85f);
        float s = (saw1 * 0.55f + saw2 * 0.45f) * env * 0.8f;
        out[i] = static_cast<int16_t>(s * 32000.0f);
    }
    return out;
}

std::vector<int16_t> genMeteor() {
    const float duration = 0.65f;
    const size_t totalSamples = static_cast<size_t>(kSampleRate * duration);
    std::vector<int16_t> out(totalSamples);
    float phase = 0;
    float lpf = 0;
    for (size_t i = 0; i < totalSamples; ++i) {
        float t = static_cast<float>(i) / kSampleRate;
        float progress = t / duration;
        float freq = 90.0f - 65.0f * progress;
        phase += 2.0f * kPi * freq / kSampleRate;
        float noise = (static_cast<float>(std::rand()) / RAND_MAX * 2.0f - 1.0f);
        lpf += (noise - lpf) * (0.45f - 0.35f * progress);
        float env = std::exp(-3.8f * progress);
        float s = (std::sin(phase) * 0.45f + lpf * 0.55f) * env * 0.95f;
        out[i] = static_cast<int16_t>(s * 32000.0f);
    }
    return out;
}

std::vector<int16_t> genCryoFreeze() {
    const float duration = 0.38f;
    const size_t totalSamples = static_cast<size_t>(kSampleRate * duration);
    std::vector<int16_t> out(totalSamples);
    float phase1 = 0, phase2 = 0;
    for (size_t i = 0; i < totalSamples; ++i) {
        float t = static_cast<float>(i) / kSampleRate;
        float progress = t / duration;
        // High crystalline harmonic drop
        phase1 += 2.0f * kPi * (1500.0f - 600.0f * progress) / kSampleRate;
        phase2 += 2.0f * kPi * (2200.0f - 900.0f * progress) / kSampleRate;
        float noise = (static_cast<float>(std::rand()) / RAND_MAX * 2.0f - 1.0f);
        float env = std::exp(-5.0f * progress);
        float s = (std::sin(phase1) * 0.4f + std::sin(phase2) * 0.3f + noise * 0.3f) * env * 0.8f;
        out[i] = static_cast<int16_t>(s * 32000.0f);
    }
    return out;
}

std::vector<int16_t> genPowerupCollect() {
    const float duration = 0.24f;
    const size_t totalSamples = static_cast<size_t>(kSampleRate * duration);
    std::vector<int16_t> out(totalSamples);
    float phase1 = 0, phase2 = 0;
    for (size_t i = 0; i < totalSamples; ++i) {
        float t = static_cast<float>(i) / kSampleRate;
        float progress = t / duration;
        phase1 += 2.0f * kPi * (659.25f + 400.0f * progress) / kSampleRate;
        phase2 += 2.0f * kPi * (1318.51f + 600.0f * progress) / kSampleRate;
        float env = (1.0f - progress) * 0.65f;
        float s = (std::sin(phase1) * 0.5f + std::sin(phase2) * 0.5f) * env;
        out[i] = static_cast<int16_t>(s * 32000.0f);
    }
    return out;
}

std::vector<int16_t> genComboTrigger() {
    const float duration = 0.20f;
    const size_t totalSamples = static_cast<size_t>(kSampleRate * duration);
    std::vector<int16_t> out(totalSamples);
    float phase = 0;
    for (size_t i = 0; i < totalSamples; ++i) {
        float t = static_cast<float>(i) / kSampleRate;
        float progress = t / duration;
        float freq = 450.0f + 500.0f * std::sin(progress * kPi * 4.0f);
        phase += 2.0f * kPi * freq / kSampleRate;
        float env = (1.0f - progress) * 0.7f;
        float s = std::sin(phase) * env;
        out[i] = static_cast<int16_t>(s * 32000.0f);
    }
    return out;
}

std::vector<int16_t> genVictory() {
    const float duration = 1.2f;
    const size_t totalSamples = static_cast<size_t>(kSampleRate * duration);
    std::vector<int16_t> out(totalSamples);
    const float notes[4] = {523.25f, 659.25f, 783.99f, 1046.50f}; // C5, E5, G5, C6
    float phase = 0;
    for (size_t i = 0; i < totalSamples; ++i) {
        float t = static_cast<float>(i) / kSampleRate;
        int noteIdx = std::min(3, static_cast<int>(t / (duration / 4.0f)));
        float freq = notes[noteIdx];
        phase += 2.0f * kPi * freq / kSampleRate;
        float localT = std::fmod(t, duration / 4.0f) / (duration / 4.0f);
        float env = (1.0f - localT * 0.7f) * 0.65f;
        float s = std::sin(phase) * env;
        out[i] = static_cast<int16_t>(s * 32000.0f);
    }
    return out;
}

std::vector<int16_t> genGameOver() {
    const float duration = 1.2f;
    const size_t totalSamples = static_cast<size_t>(kSampleRate * duration);
    std::vector<int16_t> out(totalSamples);
    const float notes[4] = {392.00f, 349.23f, 311.13f, 261.63f}; // G4, F4, Eb4, C4
    float phase = 0;
    for (size_t i = 0; i < totalSamples; ++i) {
        float t = static_cast<float>(i) / kSampleRate;
        int noteIdx = std::min(3, static_cast<int>(t / (duration / 4.0f)));
        float freq = notes[noteIdx];
        phase += 2.0f * kPi * freq / kSampleRate;
        float localT = std::fmod(t, duration / 4.0f) / (duration / 4.0f);
        float env = (1.0f - localT * 0.8f) * 0.6f;
        float s = std::sin(phase) * env;
        out[i] = static_cast<int16_t>(s * 32000.0f);
    }
    return out;
}

}  // namespace

AudioManager::AudioManager() = default;

AudioManager::~AudioManager() {
    shutdown();
}

void AudioManager::init() {
    if (ready_) return;
    InitAudioDevice();
    if (!IsAudioDeviceReady()) {
        ready_ = false;
        return;
    }
    ready_ = true;
    generateSounds();
}

void AudioManager::generateSounds() {
    if (!ready_) return;
    auto store = [this](SoundId id, const std::vector<int16_t>& samples) {
        Sound s = createProceduralSound(samples);
        Sound* heapSound = new Sound(s);
        sounds_[static_cast<size_t>(id)] = heapSound;
    };

    store(SoundId::Shoot, genShoot());
    store(SoundId::Laser, genLaser());
    store(SoundId::Hit, genHit());
    store(SoundId::Explosion, genExplosion());
    store(SoundId::Build, genBuild());
    store(SoundId::Upgrade, genUpgrade());
    store(SoundId::Sell, genSell());
    store(SoundId::Repair, genRepair());
    store(SoundId::Overdrive, genOverdrive());
    store(SoundId::TitanRoar, genTitanRoar());
    store(SoundId::SurgeHorn, genSurgeHorn());
    store(SoundId::Meteor, genMeteor());
    store(SoundId::CryoFreeze, genCryoFreeze());
    store(SoundId::PowerupCollect, genPowerupCollect());
    store(SoundId::ComboTrigger, genComboTrigger());
    store(SoundId::Victory, genVictory());
    store(SoundId::GameOver, genGameOver());
}

void AudioManager::shutdown() {
    if (!ready_) return;
    for (size_t i = 0; i < static_cast<size_t>(SoundId::Count); ++i) {
        if (sounds_[i]) {
            Sound* s = static_cast<Sound*>(sounds_[i]);
            UnloadSound(*s);
            delete s;
            sounds_[i] = nullptr;
        }
    }
    CloseAudioDevice();
    ready_ = false;
}

void AudioManager::play(SoundId id, float pitch, float volumeScale) {
    if (!ready_ || muted_) return;
    size_t idx = static_cast<size_t>(id);
    if (idx >= static_cast<size_t>(SoundId::Count)) return;
    Sound* s = static_cast<Sound*>(sounds_[idx]);
    if (!s) return;

    SetSoundVolume(*s, volume_ * volumeScale);
    SetSoundPitch(*s, pitch);
    PlaySound(*s);
}

void AudioManager::toggleMute() {
    muted_ = !muted_;
}

void AudioManager::setVolume(float v) {
    volume_ = std::max(0.0f, std::min(1.0f, v));
}

}  // namespace tf
