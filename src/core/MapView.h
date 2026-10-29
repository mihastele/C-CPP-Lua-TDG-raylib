// towerfall — the coordinate bridge between the simulation and the screen.
//
// THE SINGLE SOURCE OF TRUTH FOR THE COORDINATE CONVERSION.
//
// The simulation works in a fixed world space where one tile is `worldTileSize`
// pixels (48). The map is DRAWN at `tileSize` pixels per tile so that it fits the
// window, so those two numbers are usually different.
//
// Anything positioned in world pixels — enemies, projectiles, floating text — must be
// scaled by the same factor as the terrain, or it is drawn somewhere the terrain is
// not. Forgetting this is not a subtle bug: the enemy leaves the path, starts a tile
// high, and drifts off to the right, because the error grows with distance from the
// origin.
//
// This struct is shared by the renderer (world -> screen) and by the input code
// (screen -> cell), so the two can never disagree. It has no graphics dependency, so
// it is unit tested.

#pragma once

#include "core/Types.h"

namespace tf {

struct MapView {
    // Where the map's top-left corner is on the screen.
    float originX = 0;
    float originY = 0;
    // How many screen pixels one tile occupies.
    float tileSize = 48;
    // How many WORLD pixels one tile occupies (the simulation's tile size).
    float worldTileSize = 48;

    // The factor that converts world pixels into screen pixels.
    float scale() const { return tileSize / worldTileSize; }

    // world -> screen
    Vec2 worldToScreen(Vec2 w) const {
        const float s = scale();
        return Vec2{originX + w.x * s, originY + w.y * s};
    }

    // screen -> world
    Vec2 screenToWorld(float sx, float sy) const {
        const float s = worldTileSize / tileSize;
        return Vec2{(sx - originX) * s, (sy - originY) * s};
    }

    // screen -> cell
    Cell screenToCell(float sx, float sy) const {
        return Cell{static_cast<int>((sx - originX) / tileSize),
                    static_cast<int>((sy - originY) / tileSize)};
    }

    // cell -> screen (the top-left corner of the tile, for drawing terrain)
    Vec2 cellToScreen(Cell c) const {
        return Vec2{originX + c.x * tileSize, originY + c.y * tileSize};
    }
};

}  // namespace tf
