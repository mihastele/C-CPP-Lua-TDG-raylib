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
    Texture2D heart = {0};
    Texture2D coin = {0};
    Texture2D skull = {0};
    Texture2D coreIcon = {0};
};

SpriteSet buildSprites();
void freeSprites(SpriteSet& s);

// ------------------------------------------------------------------ renderer

class Renderer {
public:
    void init();
    void shutdown();
    void draw(const Game& g, const GameView& view);

private:
    SpriteSet sprites_;
};

// Helpers shared by the renderer and the sprite generator.
Color paletteColor(const std::string& name);

}  // namespace tf
