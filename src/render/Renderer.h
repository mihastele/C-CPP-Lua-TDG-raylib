// towerfall — presentation layer.
//
// Everything visual lives here: the procedural sprite generation, the renderer,
// and the view state (which cell is hovered, which tower is selected). The
// simulation never sees any of this.

#pragma once

#include <string>
#include <vector>

#include "raylib.h"

#include "core/Game.h"
#include "core/MapView.h"
#include "render/Audio.h"

namespace tf {

// What the player is currently doing.
enum class ToolMode : uint8_t {
    Select = 0,     // click a tower to inspect it
    Build,          // placing a tower of buildKind
    Sell,           // clicking a tower sells it
    Upgrade,        // clicking a tower upgrades it
};

struct GameView {
    ToolMode mode = ToolMode::Select;
    TowerKind buildKind = TowerKind::Arrow;

    Cell hover{-1, -1};
    int selectedTowerId = 0;

    bool showRanges = true;
    bool showGrid = true;
    bool showPathOverlay = false;
    bool showStatsModal = false;
    bool targetingMeteor = false;
    float time = 0;
};

// ------------------------------------------------------------------ sprites
//
// Sprites are GENERATED, not loaded. Each tower, each upgrade level and each
// enemy shape is drawn procedurally into an Image at startup. That keeps the
// project asset-free (nothing to download, nothing to license) and means a new
// tower is a few lines of drawing code rather than a PNG.

struct SpriteSet {
    // One texture per (tower kind, level).
    std::vector<Texture2D> towerSprites;
    // One texture per enemy kind.
    std::vector<Texture2D> enemySprites;
    // Icons for the UI.
    Texture2D heart{};
    Texture2D coin{};
    Texture2D skull{};
    Texture2D coreIcon{};
};

struct Particle {
    Vector2 pos{0, 0};
    Vector2 vel{0, 0};
    Color color{255, 255, 255, 255};
    float life = 0;
    float maxLife = 0.5f;
    float size = 2.0f;
};

struct Shockwave {
    Vector2 pos{0, 0};
    float radius = 0;
    float maxRadius = 30.0f;
    Color color{255, 255, 255, 255};
    float life = 0;
    float maxLife = 0.4f;
};

SpriteSet buildSprites();
void freeSprites(SpriteSet& s);

// ------------------------------------------------------------------ renderer

class Renderer {
public:
    void init();
    void shutdown();
    void draw(const Game& g, const GameView& view);

    void addExplosion(Vector2 pos, Color c, int count, float speed);
    void addShockwave(Vector2 pos, float maxR, Color c, float life);
    void triggerScreenShake(float intensity);

    AudioManager& audio() { return audio_; }
    const AudioManager& audio() const { return audio_; }

private:
    void processEvents(const Game& g, const MapView& mapView);

    AudioManager audio_;
    SpriteSet sprites_;
    std::vector<Particle> particles_;
    std::vector<Shockwave> shockwaves_;
    float screenShake_ = 0.0f;
    size_t lastProcessedEventIndex_ = 0;
};

// Helpers shared by the renderer and the sprite generator.
Color paletteColor(const std::string& name);

}  // namespace tf
