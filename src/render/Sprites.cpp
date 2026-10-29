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
// HOW THE SCALING WORKS
// ---------------------
// Every drawing function works in a LOGICAL 64x64 space and multiplies its
// coordinates by `u = size / 64`. So a 384-pixel canvas draws exactly the same
// picture as a 64-pixel one, just with more detail. The images are then
// downsampled with bilinear filtering, which is what gives the soft edges —
// drawing straight onto a 48-pixel image gives hard, aliased edges.
//
// OUTLINES
// --------
// An outline is drawn by painting the shape in the outline colour at radius r+1
// and then painting the fill at radius r on top. It is NOT done by punching a
// transparent hole, because raylib's drawing functions BLEND: a transparent
// "hole" does not clear anything, and where it does it also erases the fill.

#include "render/Sprites.h"

#include <cmath>

namespace tf {
namespace {

constexpr int kSupersample = 4;   // draw big, shrink down
constexpr float kLogical = 64.0f; // the design grid

Color rgba(int r, int g, int b, int a = 255) {
    return Color{static_cast<unsigned char>(r), static_cast<unsigned char>(g),
                 static_cast<unsigned char>(b), static_cast<unsigned char>(a)};
}

// ---- palette ------------------------------------------------------------

struct Palette {
    Color base;
    Color dark;
    Color light;
    Color accent;
};

Palette paletteFor(TowerKind kind) {
    switch (kind) {
        case TowerKind::Arrow:    return {rgba(120, 88, 56),  rgba(74, 54, 34),   rgba(176, 138, 92),  rgba(228, 226, 210)};
        case TowerKind::Cannon:   return {rgba(84, 88, 96),   rgba(48, 52, 60),   rgba(132, 138, 148), rgba(214, 138, 62)};
        case TowerKind::Frost:    return {rgba(126, 190, 214),rgba(64, 128, 164), rgba(196, 232, 248), rgba(236, 252, 255)};
        case TowerKind::Tesla:    return {rgba(96, 92, 140),  rgba(52, 48, 84),   rgba(158, 152, 210), rgba(120, 220, 255)};
        case TowerKind::Sniper:   return {rgba(92, 104, 78),  rgba(48, 58, 42),   rgba(142, 158, 118), rgba(226, 236, 180)};
        case TowerKind::Poison:   return {rgba(104, 128, 64), rgba(56, 76, 32),   rgba(158, 186, 96),  rgba(186, 240, 90)};
        case TowerKind::Flame:    return {rgba(168, 78, 52),  rgba(96, 38, 26),   rgba(232, 140, 70),  rgba(255, 214, 96)};
        case TowerKind::Mortar:   return {rgba(102, 96, 88),  rgba(56, 52, 48),   rgba(154, 148, 136), rgba(226, 190, 90)};
        case TowerKind::Ballista: return {rgba(138, 104, 64), rgba(80, 58, 36),   rgba(190, 156, 106), rgba(220, 226, 232)};
        case TowerKind::Tarpit:   return {rgba(64, 56, 44),   rgba(34, 30, 24),   rgba(96, 86, 68),   rgba(122, 190, 88)};
        case TowerKind::Beacon:   return {rgba(178, 146, 62), rgba(96, 76, 30),   rgba(226, 200, 116), rgba(255, 236, 168)};
        default:                   return {rgba(120, 120, 120),rgba(70, 70, 70),   rgba(170, 170, 170), rgba(230, 230, 230)};
    }
}

// ---- drawing helpers ----------------------------------------------------

void disc(Image& img, int cx, int cy, int r, Color c) {
    if (r <= 0) return;
    ImageDrawCircle(&img, cx, cy, r, c);
}

/** A filled circle with an outline: outline first, then the fill on top. */
void outlinedDisc(Image& img, int cx, int cy, int r, Color fill, Color outline) {
    disc(img, cx, cy, r + 1, outline);
    disc(img, cx, cy, r, fill);
}

void rect(Image& img, int x, int y, int w, int h, Color c) {
    if (w <= 0 || h <= 0) return;
    ImageDrawRectangle(&img, x, y, w, h, c);
}

/** A pillar with a shaded right side. */
void pillar(Image& img, int cx, int fromY, int toY, int halfWidth, Color main, Color shade) {
    rect(img, cx - halfWidth, fromY, halfWidth * 2, toY - fromY, main);
    rect(img, cx + halfWidth - halfWidth / 2, fromY, halfWidth / 2, toY - fromY, shade);
}

/** A barrel with a shaded right side. */
void barrel(Image& img, int cx, int cy, int halfWidth, int height, Color main, Color shade) {
    rect(img, cx - halfWidth, cy - height / 2, halfWidth * 2, height, main);
    rect(img, cx + halfWidth - halfWidth / 2, cy - height / 2, halfWidth / 2, height, shade);
}

// ---------------------------------------------------------------- towers
//
// Every tower is drawn inside a 64x64 logical box that fills the sprite.
// The ground line sits at y=58, so towers stand on the lower edge.

void drawTowerBase(Image& img, float u, const Palette& p, int level) {
    const int c = static_cast<int>(32 * u);
    const int groundY = static_cast<int>(58 * u);

    // A shadow on the ground.
    disc(img, c, groundY, static_cast<int>(22 * u), rgba(0, 0, 0, 55));

    // The plinth. Higher levels get a taller, sturdier base.
    const int plinthH = static_cast<int>((13 + level * 3) * u);
    const int plinthW = static_cast<int>((17 + level * 2) * u);
    pillar(img, c, groundY - plinthH, groundY, plinthW, p.base, p.dark);

    // A lighter band along the top of the plinth.
    rect(img, c - plinthW, groundY - plinthH - static_cast<int>(2 * u),
         plinthW * 2, static_cast<int>(3 * u), p.light);
}

void drawArrowTower(Image& img, float u, const Palette& p, int level) {
    const int c = static_cast<int>(32 * u);
    const int groundY = static_cast<int>(58 * u);
    drawTowerBase(img, u, p, level);

    // The archer platform.
    const int deckY = groundY - static_cast<int>((13 + level * 3) * u);
    rect(img, c - static_cast<int>(13 * u), deckY, static_cast<int>(26 * u),
         static_cast<int>(4 * u), p.light);
    rect(img, c - static_cast<int>(13 * u), deckY + static_cast<int>(4 * u),
         static_cast<int>(26 * u), static_cast<int>(3 * u), p.dark);

    // The archer.
    const int headY = deckY - static_cast<int>(4 * u);
    disc(img, c, headY, static_cast<int>(4 * u), p.accent);
    rect(img, c - static_cast<int>(3 * u), headY, static_cast<int>(6 * u),
         static_cast<int>(5 * u), p.dark);
    if (level >= 3) {
        rect(img, c - static_cast<int>(5 * u), headY - static_cast<int>(4 * u),
             static_cast<int>(10 * u), static_cast<int>(3 * u), p.light);
    }

    // The bow.
    const int bowX = c + static_cast<int>(8 * u);
    const int bowR = static_cast<int>((8 + level) * u);
    for (int a = -70; a <= 70; a += 4) {
        const float rad = a * 3.14159f / 180.0f;
        const int px = bowX + static_cast<int>(std::sin(rad) * bowR);
        const int py = headY + static_cast<int>(std::cos(rad) * bowR);
        disc(img, px, py, std::max(1, static_cast<int>(u)), p.accent);
    }
    ImageDrawLine(&img, bowX - static_cast<int>(u), headY - bowR,
                  bowX - static_cast<int>(u), headY + bowR, p.accent);
}

void drawCannonTower(Image& img, float u, const Palette& p, int level) {
    const int c = static_cast<int>(32 * u);
    const int groundY = static_cast<int>(58 * u);
    drawTowerBase(img, u, p, level);

    // The barrel, angled up to the right.
    const int bx = c;
    const int by = groundY - static_cast<int>((26 + level * 3) * u);
    const int len = static_cast<int>((26 + level * 4) * u);
    const int w = static_cast<int>((11 + level * 2) * u);
    for (int i = 0; i < len; ++i) {
        rect(img, bx + i - w / 2, by + i / 3, w, w, i > len * 0.7 ? p.dark : p.base);
    }
    disc(img, bx + len - w / 2, by + len / 3, w / 2 + static_cast<int>(2 * u), p.dark);
    disc(img, bx + len - w / 2, by + len / 3, w / 4, p.accent);

    // The wheels.
    const int wheelY = groundY - static_cast<int>(9 * u);
    outlinedDisc(img, c - static_cast<int>(11 * u), wheelY, static_cast<int>(7 * u), p.dark, p.base);
    outlinedDisc(img, c + static_cast<int>(11 * u), wheelY, static_cast<int>(7 * u), p.dark, p.base);
    disc(img, c - static_cast<int>(11 * u), wheelY, static_cast<int>(2 * u), p.accent);
    disc(img, c + static_cast<int>(11 * u), wheelY, static_cast<int>(2 * u), p.accent);
}

void drawFrostTower(Image& img, float u, const Palette& p, int level) {
    const int c = static_cast<int>(32 * u);
    const int groundY = static_cast<int>(58 * u);
    drawTowerBase(img, u, p, level);

    // A crystal spire: three stacked shards.
    const int baseY = groundY - static_cast<int>((13 + level * 3) * u);
    for (int i = 0; i < 3; ++i) {
        const int h = static_cast<int>((14 + i * 9 + level * 3) * u);
        const int w = static_cast<int>((11 - i * 3) * u);
        const int ox = static_cast<int>((i - 1) * 7 * u);
        for (int y = 0; y < h; ++y) {
            const int shrink = (y * w) / h;
            rect(img, c + ox - w + shrink, baseY - h + y, (w - shrink) * 2,
                 std::max(1, static_cast<int>(u)), y > h / 2 ? p.light : p.base);
        }
    }
    // A bright tip.
    disc(img, c, baseY - static_cast<int>((38 + level * 3) * u),
         static_cast<int>((3 + level) * u), p.accent);
}

void drawTeslaTower(Image& img, float u, const Palette& p, int level) {
    const int c = static_cast<int>(32 * u);
    const int groundY = static_cast<int>(58 * u);
    drawTowerBase(img, u, p, level);

    // A coil: stacked rings around a core.
    const int topY = groundY - static_cast<int>((14 + level * 3) * u);
    const int coils = 4 + level;
    for (int i = 0; i < coils; ++i) {
        const int y = topY - i * static_cast<int>(5 * u);
        const int w = static_cast<int>((15 - i * 1.6f) * u);
        rect(img, c - w, y, w * 2, std::max(1, static_cast<int>(3 * u)),
             i % 2 ? p.light : p.base);
    }
    // The core.
    const int coreY = topY - static_cast<int>((coils * 5 + 4) * u);
    outlinedDisc(img, c, coreY, static_cast<int>((6 + level) * u), p.accent, p.dark);
    disc(img, c, coreY, static_cast<int>((2 + level) * u), Color{255, 255, 255, 255});
}

void drawSniperTower(Image& img, float u, const Palette& p, int level) {
    const int c = static_cast<int>(32 * u);
    const int groundY = static_cast<int>(58 * u);
    drawTowerBase(img, u, p, level);

    // A long, thin barrel pointing right.
    const int by = groundY - static_cast<int>((32 + level * 3) * u);
    const int len = static_cast<int>((30 + level * 5) * u);
    rect(img, c - static_cast<int>(6 * u), by, len, static_cast<int>(5 * u), p.base);
    rect(img, c - static_cast<int>(6 * u), by + static_cast<int>(5 * u), len,
         static_cast<int>(2 * u), p.dark);
    // The scope.
    outlinedDisc(img, c - static_cast<int>(10 * u), by - static_cast<int>(4 * u),
                 static_cast<int>(6 * u), p.light, p.dark);
    disc(img, c - static_cast<int>(10 * u), by - static_cast<int>(4 * u),
         static_cast<int>(2 * u), p.accent);
    // A bipod.
    ImageDrawLine(&img, c + static_cast<int>(16 * u), by + static_cast<int>(7 * u),
                  c + static_cast<int>(11 * u), groundY, p.dark);
    ImageDrawLine(&img, c + static_cast<int>(16 * u), by + static_cast<int>(7 * u),
                  c + static_cast<int>(21 * u), groundY, p.dark);
}

void drawPoisonTower(Image& img, float u, const Palette& p, int level) {
    const int c = static_cast<int>(32 * u);
    const int groundY = static_cast<int>(58 * u);
    drawTowerBase(img, u, p, level);

    // A bubbling vat.
    const int vy = groundY - static_cast<int>((16 + level * 2) * u);
    const int vw = static_cast<int>((15 + level * 2) * u);
    const int vh = static_cast<int>((20 + level * 3) * u);
    barrel(img, c, vy, vw, vh, p.base, p.dark);
    rect(img, c - vw, vy - static_cast<int>(4 * u), vw * 2, static_cast<int>(5 * u), p.light);
    // Bubbles rising.
    disc(img, c - static_cast<int>(5 * u), vy - static_cast<int>((10 + level) * u),
         static_cast<int>(3 * u), p.accent);
    disc(img, c + static_cast<int>(6 * u), vy - static_cast<int>((16 + level) * u),
         static_cast<int>(4 * u), p.accent);
    disc(img, c, vy - static_cast<int>((22 + level) * u), static_cast<int>(3 * u), p.accent);
}

void drawFlameTower(Image& img, float u, const Palette& p, int level) {
    const int c = static_cast<int>(32 * u);
    const int groundY = static_cast<int>(58 * u);
    drawTowerBase(img, u, p, level);

    // A brazier.
    const int fy = groundY - static_cast<int>((13 + level * 2) * u);
    barrel(img, c, fy, static_cast<int>((13 + level * 2) * u),
           static_cast<int>(12 * u), p.base, p.dark);
    // Flames.
    const int flames = 3 + level;
    for (int i = 0; i < flames; ++i) {
        const int h = static_cast<int>((11 + i * 5) * u);
        const int w = static_cast<int>((6 + i) * u);
        const int ox = static_cast<int>((i % 2 ? 1 : -1) * i * 3 * u);
        rect(img, c + ox - w, fy - static_cast<int>(6 * u) - h, w * 2, h,
             i == 0 ? p.accent : p.light);
    }
}

void drawPrismTower(Image& img, float u, const Palette& p, int level) {
    const int c = static_cast<int>(32 * u);
    const int groundY = static_cast<int>(58 * u);
    drawTowerBase(img, u, p, level);

    // A floating faceted crystal.
    const int cy = groundY - static_cast<int>((30 + level * 3) * u);
    const int r = static_cast<int>((13 + level * 2) * u);
    for (int y = -r; y <= r; ++y) {
        const int half = static_cast<int>(r * std::sqrt(std::max(0.0f, 1.0f -
                                          (y * y) / (float)(r * r))));
        rect(img, c - half, cy + y, half * 2, std::max(1, static_cast<int>(u)),
             y < 0 ? p.light : p.base);
    }
    // The beams splitting out of it.
    ImageDrawLine(&img, c, cy, c + static_cast<int>(20 * u), cy - static_cast<int>(18 * u), p.accent);
    ImageDrawLine(&img, c, cy, c - static_cast<int>(20 * u), cy - static_cast<int>(18 * u), p.accent);
    ImageDrawLine(&img, c, cy, c, cy - static_cast<int>(24 * u), p.accent);
}

void drawMortarTower(Image& img, float u, const Palette& p, int level) {
    const int c = static_cast<int>(32 * u);
    const int groundY = static_cast<int>(58 * u);
    drawTowerBase(img, u, p, level);

    // A wide, squat tube, angled steeply.
    const int bx = c - static_cast<int>(4 * u);
    const int by = groundY - static_cast<int>((16 + level * 2) * u);
    const int w = static_cast<int>((16 + level * 2) * u);
    for (int i = 0; i < static_cast<int>(22 * u); ++i) {
        rect(img, bx - i / 2, by - i, w, w, i > static_cast<int>(16 * u) ? p.dark : p.base);
    }
    outlinedDisc(img, bx - static_cast<int>(11 * u), by - static_cast<int>(22 * u),
                 w / 2, p.dark, p.base);
    disc(img, bx - static_cast<int>(11 * u), by - static_cast<int>(22 * u),
         static_cast<int>(3 * u), p.accent);
    // Ammunition beside it.
    for (int i = 0; i < 3; ++i) {
        disc(img, c + static_cast<int>((16 + i * 5) * u), groundY - static_cast<int>(6 * u),
             static_cast<int>(3 * u), p.accent);
    }
}

void drawBallistaTower(Image& img, float u, const Palette& p, int level) {
    const int c = static_cast<int>(32 * u);
    const int groundY = static_cast<int>(58 * u);
    drawTowerBase(img, u, p, level);

    // A heavy bow with a drawn string.
    const int by = groundY - static_cast<int>((28 + level * 3) * u);
    const int bowR = static_cast<int>((17 + level * 2) * u);
    for (int a = -80; a <= 80; a += 3) {
        const float rad = a * 3.14159f / 180.0f;
        const int px = c + static_cast<int>(std::sin(rad) * bowR);
        const int py = by + static_cast<int>(std::cos(rad) * bowR);
        disc(img, px, py, std::max(1, static_cast<int>(u)), p.accent);
    }
    ImageDrawLine(&img, c, by - bowR, c, by + bowR, p.accent);
    // The bolt.
    rect(img, c - static_cast<int>(3 * u), by - static_cast<int>(2 * u),
         static_cast<int>((24 + level * 4) * u), static_cast<int>(4 * u), p.light);
}

void drawTarpitTower(Image& img, float u, const Palette& p, int level) {
    const int c = static_cast<int>(32 * u);
    const int groundY = static_cast<int>(58 * u);
    // No plinth: it is a pool, not a building.
    const int r = static_cast<int>((22 + level * 3) * u);
    outlinedDisc(img, c, groundY - static_cast<int>(4 * u), r, p.base, p.dark);
    disc(img, c, groundY - static_cast<int>(4 * u), r - static_cast<int>(4 * u), p.light);
    // Bubbles in the tar.
    disc(img, c - static_cast<int>(8 * u), groundY - static_cast<int>(8 * u),
         static_cast<int>(3 * u), p.accent);
    disc(img, c + static_cast<int>(9 * u), groundY - static_cast<int>(6 * u),
         static_cast<int>(4 * u), p.accent);
}

void drawBeaconTower(Image& img, float u, const Palette& p, int level) {
    const int c = static_cast<int>(32 * u);
    const int groundY = static_cast<int>(58 * u);
    drawTowerBase(img, u, p, level);

    // A tall staff with a glowing lamp.
    const int topY = groundY - static_cast<int>((18 + level * 4) * u);
    rect(img, c - static_cast<int>(3 * u), topY, static_cast<int>(6 * u),
         groundY - topY, p.dark);
    // The lamp housing.
    for (int i = 0; i < 7 + level * 2; ++i) {
        const int w = static_cast<int>((12 - i * 0.8f) * u);
        rect(img, c - w, topY - static_cast<int>((12 + i) * u), w * 2,
             std::max(1, static_cast<int>(u)), i < 3 ? p.accent : p.light);
    }
    // Radiating light.
    const int glowR = static_cast<int>((14 + level * 2) * u);
    for (int a = 0; a < 360; a += 45) {
        const float rad = a * 3.14159f / 180.0f;
        disc(img, c + static_cast<int>(std::cos(rad) * glowR),
             topY - static_cast<int>(14 * u) + static_cast<int>(std::sin(rad) * glowR),
             std::max(1, static_cast<int>(u)), rgba(255, 236, 168, 150));
    }
}

}  // namespace

Image renderTowerSprite(TowerKind kind, int level, int size) {
    const int big = size * kSupersample;
    Image img = GenImageColor(big, big, Color{0, 0, 0, 0});
    const Palette p = paletteFor(kind);
    const float u = big / kLogical;

    switch (kind) {
        case TowerKind::Arrow:    drawArrowTower(img, u, p, level); break;
        case TowerKind::Cannon:   drawCannonTower(img, u, p, level); break;
        case TowerKind::Frost:    drawFrostTower(img, u, p, level); break;
        case TowerKind::Tesla:    drawTeslaTower(img, u, p, level); break;
        case TowerKind::Sniper:   drawSniperTower(img, u, p, level); break;
        case TowerKind::Poison:   drawPoisonTower(img, u, p, level); break;
        case TowerKind::Flame:    drawFlameTower(img, u, p, level); break;
        case TowerKind::Prism:    drawPrismTower(img, u, p, level); break;
        case TowerKind::Mortar:   drawMortarTower(img, u, p, level); break;
        case TowerKind::Ballista: drawBallistaTower(img, u, p, level); break;
        case TowerKind::Tarpit:   drawTarpitTower(img, u, p, level); break;
        case TowerKind::Beacon:   drawBeaconTower(img, u, p, level); break;
        default: break;
    }

    ImageResizeNN(&img, size, size);
    return img;
}

}  // namespace tf
