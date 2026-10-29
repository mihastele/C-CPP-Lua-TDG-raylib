// towerfall — procedural sprite generation.
//
// Every sprite in the game is DRAWN, not loaded. There are no image files: a
// tower is a function that draws a base, a turret and some detail, and the
// upgrade levels add more. That means:
//
//   - the game ships with no assets to download or license
//   - a new tower is a few lines of drawing code
//   - everything scales to any resolution
//
// The sprites are rendered at 4x and downsampled with bilinear filtering,
// because drawing straight onto a 48-pixel image gives hard, aliased edges.

#pragma once

#include <string>
#include <vector>

#include "raylib.h"

#include "core/Defs.h"
#include "core/Types.h"

namespace tf {

/** Renders one tower sprite at the given upgrade level (1..3). */
Image renderTowerSprite(TowerKind kind, int level, int size);

/** Renders one enemy sprite. `shape` selects the body; `scale` is the size. */
Image renderEnemySprite(EnemyKind kind, const std::string& shape,
                        const std::string& color, float scale, int size);

/** A small heart, used for the life counter. */
Image renderHeartSprite(int size);

/** A coin, used for the gold counter. */
Image renderCoinSprite(int size);

/** A skull, used for the kill counter. */
Image renderSkullSprite(int size);

/** The core / base marker at the exit. */
Image renderCoreSprite(int size);

/** A small round button icon for the tower shop. */
Image renderTowerIcon(TowerKind kind, int size);

}  // namespace tf
