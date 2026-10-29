// towerfall — procedural sprite generation (top-view).
//
// Every sprite in the game is PROCEDURALLY DRAWN, not loaded.
// There are no external image files: zero compute credits required,
// 100% deterministic, instant startup, scalable to any resolution,
// and mathematically bounded so sprites NEVER overflow the canvas.
//
// Top-view design:
//   - Centered strictly at (32, 32) in logical 64x64 design space.
//   - Strict bounding radius R <= 26 units (safe 6-pixel perimeter padding).
//   - Directional towers and weapons point East (0 degrees / +X) so they
//     can rotate seamlessly to face targets.
//   - Level 1 -> 2 -> 3 upgrades feature distinct structural tiers:
//     Lv1: Sturdy stone/wood base, single weapon/mechanism, bronze studs.
//     Lv2: Reinforced steel battlements, dual barrels/prongs, silver trim.
//     Lv3: Masterwork gilded fortifications, heavy quad arrays, glowing
//          arcane power conduits and radiant cores.

#include "render/Sprites.h"

#include <algorithm>
#include <cmath>

namespace tf {
namespace {

constexpr int kSupersample = 4;   // 4x supersampling for soft, antialiased edges
constexpr float kLogical = 64.0f; // logical design space

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
        case TowerKind::Arrow:    return {rgba(125, 92, 58),   rgba(68, 48, 30),   rgba(182, 144, 98),  rgba(235, 230, 215)};
        case TowerKind::Cannon:   return {rgba(86, 90, 98),    rgba(46, 50, 58),   rgba(138, 144, 154), rgba(225, 145, 65)};
        case TowerKind::Frost:    return {rgba(120, 192, 220), rgba(58, 120, 158), rgba(200, 236, 252), rgba(240, 252, 255)};
        case TowerKind::Tesla:    return {rgba(94, 90, 142),   rgba(50, 46, 82),   rgba(160, 154, 215), rgba(125, 225, 255)};
        case TowerKind::Sniper:   return {rgba(90, 106, 78),   rgba(46, 58, 40),   rgba(146, 162, 122), rgba(230, 240, 185)};
        case TowerKind::Poison:   return {rgba(102, 130, 62),  rgba(54, 76, 30),   rgba(160, 190, 95),  rgba(190, 245, 90)};
        case TowerKind::Flame:    return {rgba(172, 78, 50),   rgba(95, 36, 24),   rgba(236, 142, 68),  rgba(255, 220, 95)};
        case TowerKind::Mortar:   return {rgba(104, 98, 90),   rgba(55, 50, 46),   rgba(156, 150, 138), rgba(230, 195, 95)};
        case TowerKind::Ballista: return {rgba(140, 105, 65),  rgba(78, 56, 34),   rgba(195, 160, 110), rgba(225, 230, 235)};
        case TowerKind::Tarpit:   return {rgba(62, 54, 42),    rgba(32, 28, 22),   rgba(98, 88, 70),    rgba(125, 195, 90)};
        case TowerKind::Beacon:   return {rgba(182, 148, 60),  rgba(98, 76, 28),   rgba(230, 204, 118), rgba(255, 240, 175)};
        default:                  return {rgba(120, 120, 120), rgba(70, 70, 70),   rgba(170, 170, 170), rgba(230, 230, 230)};
    }
}

// ---- drawing helpers ----------------------------------------------------

void disc(Image& img, int cx, int cy, int r, Color c) {
    if (r <= 0) return;
    ImageDrawCircle(&img, cx, cy, r, c);
}

void outlinedDisc(Image& img, int cx, int cy, int r, Color fill, Color outline) {
    disc(img, cx, cy, r + 1, outline);
    disc(img, cx, cy, r, fill);
}

void rect(Image& img, int x, int y, int w, int h, Color c) {
    if (w <= 0 || h <= 0) return;
    ImageDrawRectangle(&img, x, y, w, h, c);
}

void line(Image& img, int x1, int y1, int x2, int y2, Color c) {
    ImageDrawLine(&img, x1, y1, x2, y2, c);
}

// Draws a solid top-view fortified circular plinth centered at (c, c)
// with distinct upgrade tiers (Lv1 stone, Lv2 iron trim, Lv3 gilded gold/runes).
void drawBasePlatform(Image& img, float u, const Palette& p, int level) {
    const int c = static_cast<int>(32 * u);
    const int rBase = static_cast<int>((20.5f + level * 1.5f) * u); // Lv1: 22u, Lv2: 23.5u, Lv3: 25u

    // Drop shadow under foundation
    disc(img, c, c, rBase + static_cast<int>(1.5f * u), rgba(15, 15, 22, 130));

    // Outer foundation rim
    disc(img, c, c, rBase, p.dark);
    disc(img, c, c, rBase - static_cast<int>(2.0f * u), p.base);
    disc(img, c, c, rBase - static_cast<int>(4.0f * u), p.dark);

    // Inner deck platform floor
    const int rDeck = static_cast<int>((15.0f + level * 1.0f) * u);
    disc(img, c, c, rDeck, p.light);
    disc(img, c, c, rDeck - static_cast<int>(1.5f * u), p.base);

    // Crenellations / battlements / corner brackets
    const int nBrackets = (level == 1) ? 4 : 8;
    const float step = 6.2831853f / nBrackets;
    const Color studColor = (level == 3) ? rgba(255, 220, 100, 255) :
                            (level == 2) ? rgba(220, 225, 235, 255) : p.accent;

    for (int i = 0; i < nBrackets; ++i) {
        const float a = i * step + (level == 1 ? 0.785398f : 0.0f);
        const int bx = c + static_cast<int>(std::cos(a) * (rBase - 2.0f * u));
        const int by = c + static_cast<int>(std::sin(a) * (rBase - 2.0f * u));
        disc(img, bx, by, static_cast<int>(1.8f * u), p.dark);
        disc(img, bx, by, std::max(1, static_cast<int>(1.0f * u)), studColor);
    }

    // Level 3 gets an arcane golden runic inner ring
    if (level >= 3) {
        for (int a = 0; a < 360; a += 45) {
            const float rad = a * 0.0174533f;
            const int rx = c + static_cast<int>(std::cos(rad) * (rDeck - 2.5f * u));
            const int ry = c + static_cast<int>(std::sin(rad) * (rDeck - 2.5f * u));
            disc(img, rx, ry, std::max(1, static_cast<int>(0.9f * u)), p.accent);
        }
    }

    // Central turret pivot ring
    const int rPivot = static_cast<int>((8.5f + level * 0.8f) * u);
    outlinedDisc(img, c, c, rPivot, p.light, p.dark);
}

// ---------------------------------------------------------------- towers

void drawArrowTower(Image& img, float u, const Palette& p, int level) {
    const int c = static_cast<int>(32 * u);
    drawBasePlatform(img, u, p, level);

    // Central wood turntable mount
    disc(img, c, c, static_cast<int>(7.5f * u), p.dark);
    disc(img, c, c, static_cast<int>(6.0f * u), p.base);

    // Crossbow track extending forward (+X)
    const int stockLen = static_cast<int>((18 + level * 2) * u);
    rect(img, c - static_cast<int>(5 * u), c - static_cast<int>(2 * u),
         stockLen, static_cast<int>(4 * u), p.dark);
    rect(img, c - static_cast<int>(4 * u), c - static_cast<int>(1 * u),
         stockLen - static_cast<int>(2 * u), static_cast<int>(2 * u), p.light);

    // Bow limbs curving outward
    const int bowX = c + static_cast<int>(7 * u);
    const int bowSpan = static_cast<int>((11 + level * 2) * u);

    // Main composite bow limbs
    line(img, bowX, c, bowX - static_cast<int>(4 * u), c - bowSpan, p.dark);
    line(img, bowX, c, bowX - static_cast<int>(4 * u), c + bowSpan, p.dark);
    line(img, bowX + static_cast<int>(1 * u), c, bowX - static_cast<int>(3 * u), c - bowSpan, p.accent);
    line(img, bowX + static_cast<int>(1 * u), c, bowX - static_cast<int>(3 * u), c + bowSpan, p.accent);

    // Drawn bowstring
    line(img, bowX - static_cast<int>(4 * u), c - bowSpan, c - static_cast<int>(3 * u), c, rgba(230, 230, 220, 210));
    line(img, bowX - static_cast<int>(4 * u), c + bowSpan, c - static_cast<int>(3 * u), c, rgba(230, 230, 220, 210));

    // Level 2+ adds a secondary stacked power bow
    if (level >= 2) {
        const int subX = c + static_cast<int>(4 * u);
        const int subSpan = static_cast<int>(bowSpan * 0.75f);
        line(img, subX, c, subX - static_cast<int>(3 * u), c - subSpan, p.light);
        line(img, subX, c, subX - static_cast<int>(3 * u), c + subSpan, p.light);
    }

    // Loaded heavy arrow/bolt pointing East
    const int arrowTipX = c + stockLen - static_cast<int>(1 * u);
    line(img, c - static_cast<int>(2 * u), c, arrowTipX, c, p.accent);
    disc(img, arrowTipX, c, static_cast<int>(1.8f * u), p.accent);

    // Level 3 gets golden limb reinforcement brackets and glowing arrow tip
    if (level >= 3) {
        disc(img, bowX - static_cast<int>(4 * u), c - bowSpan, static_cast<int>(2 * u), rgba(255, 220, 90, 255));
        disc(img, bowX - static_cast<int>(4 * u), c + bowSpan, static_cast<int>(2 * u), rgba(255, 220, 90, 255));
        disc(img, arrowTipX, c, static_cast<int>(2.5f * u), rgba(255, 250, 200, 255));
    }
}

void drawCannonTower(Image& img, float u, const Palette& p, int level) {
    const int c = static_cast<int>(32 * u);
    drawBasePlatform(img, u, p, level);

    // Heavy iron carriage swivel
    disc(img, c, c, static_cast<int>((8.0f + level * 0.8f) * u), p.dark);
    disc(img, c, c, static_cast<int>((6.0f + level * 0.6f) * u), p.base);

    if (level == 1) {
        // Single heavy cannon barrel pointing East
        const int bLen = static_cast<int>(20 * u);
        const int bHalfW = static_cast<int>(3.5f * u);
        rect(img, c - static_cast<int>(4 * u), c - bHalfW, bLen, bHalfW * 2, p.dark);
        rect(img, c - static_cast<int>(3 * u), c - bHalfW + static_cast<int>(1 * u),
             bLen - static_cast<int>(3 * u), (bHalfW - static_cast<int>(1 * u)) * 2, p.base);
        // Brass reinforcement rings
        rect(img, c + static_cast<int>(5 * u), c - bHalfW - static_cast<int>(1 * u),
             static_cast<int>(2 * u), (bHalfW + static_cast<int>(1 * u)) * 2, p.accent);
        rect(img, c + bLen - static_cast<int>(3 * u), c - bHalfW - static_cast<int>(1 * u),
             static_cast<int>(2 * u), (bHalfW + static_cast<int>(1 * u)) * 2, p.dark);
        // Dark bore opening at muzzle
        disc(img, c + bLen - static_cast<int>(1 * u), c, static_cast<int>(2.5f * u), rgba(15, 15, 20, 255));
    } else if (level == 2) {
        // Twin heavy rifled barrels
        const int bLen = static_cast<int>(22 * u);
        const int bHalfW = static_cast<int>(2.5f * u);
        for (int offY : {-static_cast<int>(3.5f * u), static_cast<int>(3.5f * u)}) {
            rect(img, c - static_cast<int>(4 * u), c + offY - bHalfW, bLen, bHalfW * 2, p.dark);
            rect(img, c - static_cast<int>(3 * u), c + offY - bHalfW + static_cast<int>(1 * u),
                 bLen - static_cast<int>(3 * u), (bHalfW - static_cast<int>(1 * u)) * 2, p.base);
            rect(img, c + static_cast<int>(8 * u), c + offY - bHalfW - static_cast<int>(0.8f * u),
                 static_cast<int>(2 * u), bHalfW * 2 + static_cast<int>(1.6f * u), p.accent);
            disc(img, c + bLen - static_cast<int>(1 * u), c + offY, static_cast<int>(2.0f * u), rgba(15, 15, 20, 255));
        }
    } else {
        // Level 3: Massive triple-barrel Gatling/siege cluster with vented muzzle
        const int bLen = static_cast<int>(23 * u);
        const int bHalfW = static_cast<int>(2.2f * u);
        for (int offY : {-static_cast<int>(5.0f * u), 0, static_cast<int>(5.0f * u)}) {
            rect(img, c - static_cast<int>(5 * u), c + offY - bHalfW, bLen, bHalfW * 2, p.dark);
            rect(img, c - static_cast<int>(4 * u), c + offY - bHalfW + static_cast<int>(1 * u),
                 bLen - static_cast<int>(2 * u), (bHalfW - static_cast<int>(1 * u)) * 2, p.light);
            disc(img, c + bLen - static_cast<int>(1 * u), c + offY, static_cast<int>(2.2f * u), rgba(15, 15, 20, 255));
        }
        // Heavy brass breech plate & glowing chamber
        rect(img, c + static_cast<int>(2 * u), c - static_cast<int>(6.5f * u),
             static_cast<int>(4 * u), static_cast<int>(13.0f * u), p.accent);
        disc(img, c - static_cast<int>(4 * u), c, static_cast<int>(4.5f * u), p.accent);
    }
}

void drawFrostTower(Image& img, float u, const Palette& p, int level) {
    const int c = static_cast<int>(32 * u);
    drawBasePlatform(img, u, p, level);

    // Glowing runic snowflake channels on the platform deck
    const int rRune = static_cast<int>((13 + level * 2) * u);
    for (int a = 0; a < 360; a += 45) {
        const float rad = a * 0.0174533f;
        const int ex = c + static_cast<int>(std::cos(rad) * rRune);
        const int ey = c + static_cast<int>(std::sin(rad) * rRune);
        line(img, c, c, ex, ey, p.light);
        disc(img, ex, ey, std::max(1, static_cast<int>(1.2f * u)), p.accent);
    }

    // Outer ice crystal ring
    const int shards = 4 + (level - 1) * 2; // Lv1: 4, Lv2: 6, Lv3: 8
    const float shardR = (8.5f + level * 2.0f) * u;
    for (int i = 0; i < shards; ++i) {
        const float a = i * (6.2831853f / shards);
        const int sx = c + static_cast<int>(std::cos(a) * shardR);
        const int sy = c + static_cast<int>(std::sin(a) * shardR);
        disc(img, sx, sy, static_cast<int>((2.0f + level * 0.5f) * u), p.dark);
        disc(img, sx, sy, static_cast<int>((1.2f + level * 0.4f) * u), p.accent);
    }

    // Central faceted glacial crystal cluster
    const int coreR = static_cast<int>((6.0f + level * 1.5f) * u);
    disc(img, c, c, coreR + static_cast<int>(1.5f * u), rgba(120, 220, 255, 140)); // frost aura
    outlinedDisc(img, c, c, coreR, p.base, p.dark);

    // Crystal facets
    for (int a = 0; a < 360; a += 60) {
        const float rad = a * 0.0174533f;
        const int fx = c + static_cast<int>(std::cos(rad) * (coreR * 0.65f));
        const int fy = c + static_cast<int>(std::sin(rad) * (coreR * 0.65f));
        line(img, c, c, fx, fy, p.light);
    }

    // Brilliant white cryo-core
    disc(img, c, c, static_cast<int>((2.5f + level * 0.7f) * u), rgba(255, 255, 255, 255));
}

void drawTeslaTower(Image& img, float u, const Palette& p, int level) {
    const int c = static_cast<int>(32 * u);
    drawBasePlatform(img, u, p, level);

    // Ceramic insulator base ring with copper conductor plates
    const int rRing = static_cast<int>((13.5f + level * 1.5f) * u);
    disc(img, c, c, rRing, p.dark);
    disc(img, c, c, rRing - static_cast<int>(2.0f * u), p.base);

    // Concentric electromagnetic copper coils
    disc(img, c, c, static_cast<int>(10.0f * u), p.dark);
    disc(img, c, c, static_cast<int>(8.5f * u), rgba(195, 130, 60, 255)); // copper coil
    disc(img, c, c, static_cast<int>(6.5f * u), p.dark);

    // Radial capacitor prongs / discharge electrodes
    const int nElectrodes = 2 + level * 2; // Lv1: 4, Lv2: 6, Lv3: 8
    const float step = 6.2831853f / nElectrodes;
    const float prongR = (13.0f + level * 1.8f) * u;

    for (int i = 0; i < nElectrodes; ++i) {
        const float a = i * step;
        const int px = c + static_cast<int>(std::cos(a) * prongR);
        const int py = c + static_cast<int>(std::sin(a) * prongR);
        line(img, c, c, px, py, p.dark);
        line(img, c, c, px, py, p.accent);
        disc(img, px, py, static_cast<int>(1.8f * u), p.dark);
        disc(img, px, py, std::max(1, static_cast<int>(1.1f * u)), p.accent);
    }

    // Glowing plasma core with crackling energy arcs
    const int plasmaR = static_cast<int>((4.5f + level * 1.2f) * u);
    disc(img, c, c, plasmaR + static_cast<int>(2.0f * u), rgba(120, 220, 255, 160)); // aura
    disc(img, c, c, plasmaR, p.accent);
    disc(img, c, c, std::max(1, static_cast<int>(2.2f * u)), rgba(255, 255, 255, 255));

    if (level >= 3) {
        // Level 3 gets crackling lightning bolts around perimeter
        for (int a = 20; a < 360; a += 90) {
            const float rad = a * 0.0174533f;
            const int lx = c + static_cast<int>(std::cos(rad) * (rRing - 3.0f * u));
            const int ly = c + static_cast<int>(std::sin(rad) * (rRing - 3.0f * u));
            disc(img, lx, ly, static_cast<int>(1.5f * u), rgba(255, 255, 255, 240));
        }
    }
}

void drawSniperTower(Image& img, float u, const Palette& p, int level) {
    const int c = static_cast<int>(32 * u);
    drawBasePlatform(img, u, p, level);

    // Low-profile camo bunker housing
    disc(img, c, c, static_cast<int>(8.5f * u), p.dark);
    disc(img, c, c, static_cast<int>(6.5f * u), p.base);

    // Ultra-long precision rail barrel pointing East (+X)
    // Strictly bounded: c + 24u = 56u <= 58u, safe from overflowing!
    const int bLen = static_cast<int>((21 + level * 1.2f) * u);
    const int bHalfW = static_cast<int>((1.8f + level * 0.3f) * u);

    // Barrel casing
    rect(img, c - static_cast<int>(6 * u), c - bHalfW, bLen, bHalfW * 2, p.dark);
    rect(img, c - static_cast<int>(5 * u), c - bHalfW + static_cast<int>(1 * u),
         bLen - static_cast<int>(2 * u), std::max(1, (bHalfW - 1) * 2), p.light);

    // Parallel sniper scope / laser rangefinder (mounted offset in Y)
    const int scopeY = c - static_cast<int>(4.2f * u);
    rect(img, c - static_cast<int>(5 * u), scopeY - static_cast<int>(1.5f * u),
         static_cast<int>(12 * u), static_cast<int>(3 * u), p.dark);
    disc(img, c + static_cast<int>(7 * u), scopeY, static_cast<int>(1.8f * u), p.accent); // scope lens

    // Side stabilizer fins
    line(img, c + static_cast<int>(6 * u), c, c + static_cast<int>(2 * u), c - static_cast<int>(6 * u), p.dark);
    line(img, c + static_cast<int>(6 * u), c, c + static_cast<int>(2 * u), c + static_cast<int>(6 * u), p.dark);

    // Slotted muzzle brake at the tip
    const int tipX = c - static_cast<int>(6 * u) + bLen;
    rect(img, tipX - static_cast<int>(3 * u), c - bHalfW - static_cast<int>(1 * u),
         static_cast<int>(3 * u), (bHalfW + static_cast<int>(1 * u)) * 2, p.dark);
    disc(img, tipX, c, std::max(1, static_cast<int>(1.2f * u)), rgba(20, 25, 20, 255));

    // Level 2+ gets dual magnetic accelerator coil rings
    if (level >= 2) {
        rect(img, c + static_cast<int>(6 * u), c - bHalfW - static_cast<int>(1 * u),
             static_cast<int>(2 * u), (bHalfW + 1) * 2, p.accent);
        rect(img, c + static_cast<int>(11 * u), c - bHalfW - static_cast<int>(1 * u),
             static_cast<int>(2 * u), (bHalfW + 1) * 2, p.accent);
    }

    // Level 3 gets golden rails, emerald targeting laser dot at tip
    if (level >= 3) {
        rect(img, c + static_cast<int>(16 * u), c - bHalfW - static_cast<int>(1 * u),
             static_cast<int>(2 * u), (bHalfW + 1) * 2, rgba(255, 220, 90, 255));
        disc(img, tipX + static_cast<int>(1 * u), c, static_cast<int>(2.2f * u), rgba(120, 255, 120, 255));
    }
}

void drawPoisonTower(Image& img, float u, const Palette& p, int level) {
    const int c = static_cast<int>(32 * u);
    drawBasePlatform(img, u, p, level);

    // Alchemical boiling vat cauldron
    const int vatR = static_cast<int>((12.0f + level * 1.5f) * u);
    outlinedDisc(img, c, c, vatR, p.base, p.dark);

    // Glowing toxic acid sludge surface
    const int sludgeR = vatR - static_cast<int>(2.5f * u);
    disc(img, c, c, sludgeR, p.light);
    disc(img, c, c, sludgeR - static_cast<int>(2.0f * u), p.accent);

    // Boiling acid ripples and bubbles
    disc(img, c - static_cast<int>(4 * u), c - static_cast<int>(3 * u), static_cast<int>(2.5f * u), rgba(220, 255, 120, 240));
    disc(img, c + static_cast<int>(3 * u), c + static_cast<int>(4 * u), static_cast<int>(2.0f * u), rgba(220, 255, 120, 240));
    disc(img, c + static_cast<int>(5 * u), c - static_cast<int>(2 * u), static_cast<int>(1.8f * u), rgba(240, 255, 180, 255));

    // Directional brass atomizer spray nozzles pointing East (+X)
    const int nNozzles = (level == 1) ? 2 : (level == 2 ? 3 : 4);
    const int nLen = static_cast<int>((16 + level * 2) * u);

    for (int i = 0; i < nNozzles; ++i) {
        const float a = -0.4f + i * (0.8f / std::max(1, nNozzles - 1));
        const int nx = c + static_cast<int>(std::cos(a) * nLen);
        const int ny = c + static_cast<int>(std::sin(a) * nLen);
        line(img, c, c, nx, ny, p.dark);
        disc(img, nx, ny, static_cast<int>(2.0f * u), p.dark);
        disc(img, nx, ny, std::max(1, static_cast<int>(1.2f * u)), p.accent);
    }

    // Level 3 gets an arcane biohazard skull rune in center of sludge
    if (level >= 3) {
        disc(img, c, c, static_cast<int>(3.2f * u), rgba(40, 60, 20, 255));
        disc(img, c - static_cast<int>(1 * u), c - static_cast<int>(1 * u), static_cast<int>(0.9f * u), p.accent);
        disc(img, c + static_cast<int>(1 * u), c - static_cast<int>(1 * u), static_cast<int>(0.9f * u), p.accent);
    }
}

void drawFlameTower(Image& img, float u, const Palette& p, int level) {
    const int c = static_cast<int>(32 * u);
    drawBasePlatform(img, u, p, level);

    // Basalt furnace pit with thermal heat grates
    const int fR = static_cast<int>((11.5f + level * 1.5f) * u);
    disc(img, c, c, fR, p.dark);
    disc(img, c, c, fR - static_cast<int>(2.0f * u), p.base);

    // Molten combustion core
    disc(img, c, c, static_cast<int>(7.0f * u), rgba(230, 80, 30, 255));
    disc(img, c, c, static_cast<int>(4.5f * u), rgba(255, 170, 40, 255));
    disc(img, c, c, static_cast<int>(2.5f * u), rgba(255, 240, 120, 255));

    // Heavy industrial flamethrower projectors pointing East (+X)
    const int bLen = static_cast<int>((18 + level * 2) * u);
    const int bHalfW = static_cast<int>((2.2f + level * 0.4f) * u);

    if (level == 1) {
        // Single heavy flame nozzle
        rect(img, c, c - bHalfW, bLen, bHalfW * 2, p.dark);
        rect(img, c + static_cast<int>(2 * u), c - bHalfW + static_cast<int>(1 * u),
             bLen - static_cast<int>(3 * u), (bHalfW - 1) * 2, p.light);
        disc(img, c + bLen, c, static_cast<int>(3.2f * u), p.accent); // pilot flame
    } else if (level == 2) {
        // Twin angled dragon flame nozzles
        for (int offY : {-static_cast<int>(3.8f * u), static_cast<int>(3.8f * u)}) {
            rect(img, c, c + offY - bHalfW, bLen, bHalfW * 2, p.dark);
            rect(img, c + static_cast<int>(2 * u), c + offY - bHalfW + static_cast<int>(1 * u),
                 bLen - static_cast<int>(3 * u), (bHalfW - 1) * 2, p.light);
            disc(img, c + bLen, c + offY, static_cast<int>(3.0f * u), p.accent);
        }
    } else {
        // Level 3: Triple heavy inferno array with flared nozzles & glowing heat vents
        for (int offY : {-static_cast<int>(5.2f * u), 0, static_cast<int>(5.2f * u)}) {
            rect(img, c, c + offY - bHalfW, bLen, bHalfW * 2, p.dark);
            rect(img, c + static_cast<int>(2 * u), c + offY - bHalfW + static_cast<int>(1 * u),
                 bLen - static_cast<int>(3 * u), (bHalfW - 1) * 2, p.light);
            disc(img, c + bLen, c + offY, static_cast<int>(3.5f * u), rgba(255, 230, 100, 255));
        }
        // Thermal heat exhaust fins on sides
        rect(img, c - static_cast<int>(6 * u), c - static_cast<int>(9 * u),
             static_cast<int>(12 * u), static_cast<int>(2 * u), p.accent);
        rect(img, c - static_cast<int>(6 * u), c + static_cast<int>(7 * u),
             static_cast<int>(12 * u), static_cast<int>(2 * u), p.accent);
    }
}

void drawPrismTower(Image& img, float u, const Palette& p, int level) {
    const int c = static_cast<int>(32 * u);
    drawBasePlatform(img, u, p, level);

    // Alabaster pedestal with optical focus tracks
    disc(img, c, c, static_cast<int>(12.0f * u), p.dark);
    disc(img, c, c, static_cast<int>(10.0f * u), rgba(220, 225, 240, 255)); // polished marble

    // Rainbow refraction light beams radiating out
    const Color rainbow[6] = {
        rgba(255, 100, 100, 210), rgba(255, 180, 80, 210),  rgba(255, 240, 90, 210),
        rgba(100, 240, 140, 210), rgba(90, 200, 255, 210),  rgba(200, 120, 255, 210)
    };

    const int nBeams = (level == 1) ? 3 : (level == 2 ? 4 : 6);
    const float bLen = (15.0f + level * 2.0f) * u;

    for (int i = 0; i < nBeams; ++i) {
        const float a = i * (6.2831853f / nBeams);
        const int bx = c + static_cast<int>(std::cos(a) * bLen);
        const int by = c + static_cast<int>(std::sin(a) * bLen);
        line(img, c, c, bx, by, rainbow[i % 6]);
        disc(img, bx, by, static_cast<int>(1.8f * u), rainbow[i % 6]);
    }

    // Central geometric refracting crystal prism
    const int pR = static_cast<int>((5.5f + level * 1.2f) * u);
    disc(img, c, c, pR + static_cast<int>(1.5f * u), rgba(240, 220, 255, 150)); // aura
    outlinedDisc(img, c, c, pR, rgba(235, 225, 255, 255), p.dark);

    // Shimmering diamond prism facets
    for (int a = 30; a < 360; a += 60) {
        const float rad = a * 0.0174533f;
        const int fx = c + static_cast<int>(std::cos(rad) * (pR * 0.7f));
        const int fy = c + static_cast<int>(std::sin(rad) * (pR * 0.7f));
        line(img, c, c, fx, fy, rgba(255, 255, 255, 240));
    }
    disc(img, c, c, std::max(1, static_cast<int>(2.2f * u)), rgba(255, 255, 255, 255));
}

void drawMortarTower(Image& img, float u, const Palette& p, int level) {
    const int c = static_cast<int>(32 * u);
    drawBasePlatform(img, u, p, level);

    // Heavy reinforced blast ring with radial recoil shock pistons
    disc(img, c, c, static_cast<int>(12.5f * u), p.dark);
    disc(img, c, c, static_cast<int>(10.0f * u), p.base);

    // Ammunition shell racks on the sides
    disc(img, c - static_cast<int>(6 * u), c - static_cast<int>(7 * u), static_cast<int>(2.5f * u), p.dark);
    disc(img, c - static_cast<int>(6 * u), c + static_cast<int>(7 * u), static_cast<int>(2.5f * u), p.dark);
    disc(img, c - static_cast<int>(6 * u), c - static_cast<int>(7 * u), static_cast<int>(1.5f * u), p.accent);
    disc(img, c - static_cast<int>(6 * u), c + static_cast<int>(7 * u), static_cast<int>(1.5f * u), p.accent);

    if (level <= 2) {
        // Chunky wide-bore howitzer/bombard tube pointing East (+X)
        const int bLen = static_cast<int>((16 + level * 2) * u);
        const int bHalfW = static_cast<int>((5.0f + level * 0.8f) * u);

        // Barrel exterior
        rect(img, c - static_cast<int>(5 * u), c - bHalfW, bLen, bHalfW * 2, p.dark);
        rect(img, c - static_cast<int>(4 * u), c - bHalfW + static_cast<int>(1 * u),
             bLen - static_cast<int>(3 * u), (bHalfW - 1) * 2, p.base);

        // Massive muzzle bore ring
        const int muzzleX = c - static_cast<int>(5 * u) + bLen;
        outlinedDisc(img, muzzleX, c, bHalfW, p.dark, p.light);
        disc(img, muzzleX, c, bHalfW - static_cast<int>(1.8f * u), rgba(15, 15, 20, 255)); // dark bore
    } else {
        // Level 3: Devastator battery with dual heavy bombardment cannons
        const int bLen = static_cast<int>(19 * u);
        const int bHalfW = static_cast<int>(4.0f * u);
        for (int offY : {-static_cast<int>(4.8f * u), static_cast<int>(4.8f * u)}) {
            rect(img, c - static_cast<int>(5 * u), c + offY - bHalfW, bLen, bHalfW * 2, p.dark);
            rect(img, c - static_cast<int>(4 * u), c + offY - bHalfW + static_cast<int>(1 * u),
                 bLen - static_cast<int>(3 * u), (bHalfW - 1) * 2, p.light);
            const int muzzleX = c - static_cast<int>(5 * u) + bLen;
            outlinedDisc(img, muzzleX, c + offY, bHalfW, p.dark, p.accent);
            disc(img, muzzleX, c + offY, bHalfW - static_cast<int>(1.5f * u), rgba(15, 15, 20, 255));
        }
        // Golden reinforced breech block
        rect(img, c - static_cast<int>(6 * u), c - static_cast<int>(6 * u),
             static_cast<int>(5 * u), static_cast<int>(12 * u), p.accent);
    }
}

void drawBallistaTower(Image& img, float u, const Palette& p, int level) {
    const int c = static_cast<int>(32 * u);
    drawBasePlatform(img, u, p, level);

    // Timber & iron torsion carriage
    disc(img, c, c, static_cast<int>(9.0f * u), p.dark);
    disc(img, c, c, static_cast<int>(7.0f * u), p.base);

    // Central launch track pointing East (+X)
    const int railLen = static_cast<int>((21 + level * 1.5f) * u);
    rect(img, c - static_cast<int>(6 * u), c - static_cast<int>(2.2f * u),
         railLen, static_cast<int>(4.4f * u), p.dark);
    rect(img, c - static_cast<int>(5 * u), c - static_cast<int>(1.2f * u),
         railLen - static_cast<int>(2 * u), static_cast<int>(2.4f * u), p.light);

    // Heavy composite recurve bow arms
    const int bowX = c + static_cast<int>(5 * u);
    const int bowSpan = static_cast<int>((13 + level * 2) * u);

    line(img, bowX, c, bowX - static_cast<int>(5 * u), c - bowSpan, p.dark);
    line(img, bowX, c, bowX - static_cast<int>(5 * u), c + bowSpan, p.dark);
    line(img, bowX + static_cast<int>(1 * u), c, bowX - static_cast<int>(4 * u), c - bowSpan, p.base);
    line(img, bowX + static_cast<int>(1 * u), c, bowX - static_cast<int>(4 * u), c + bowSpan, p.base);

    // Dual winch cable gears
    disc(img, c - static_cast<int>(3 * u), c - static_cast<int>(4.5f * u), static_cast<int>(2.2f * u), p.dark);
    disc(img, c - static_cast<int>(3 * u), c + static_cast<int>(4.5f * u), static_cast<int>(2.2f * u), p.dark);
    disc(img, c - static_cast<int>(3 * u), c - static_cast<int>(4.5f * u), static_cast<int>(1.2f * u), p.accent);
    disc(img, c - static_cast<int>(3 * u), c + static_cast<int>(4.5f * u), static_cast<int>(1.2f * u), p.accent);

    // Steel tension cables
    line(img, bowX - static_cast<int>(5 * u), c - bowSpan, c - static_cast<int>(3 * u), c, rgba(230, 235, 240, 220));
    line(img, bowX - static_cast<int>(5 * u), c + bowSpan, c - static_cast<int>(3 * u), c, rgba(230, 235, 240, 220));

    // Massive steel harpoon projectile
    const int tipX = c - static_cast<int>(6 * u) + railLen;
    line(img, c - static_cast<int>(2 * u), c, tipX, c, rgba(240, 245, 255, 255));
    disc(img, tipX, c, static_cast<int>(2.2f * u), p.accent);

    // Level 3 gets golden leaf springs and armor-piercing tungsten lance
    if (level >= 3) {
        disc(img, bowX - static_cast<int>(5 * u), c - bowSpan, static_cast<int>(2.5f * u), rgba(255, 220, 90, 255));
        disc(img, bowX - static_cast<int>(5 * u), c + bowSpan, static_cast<int>(2.5f * u), rgba(255, 220, 90, 255));
        disc(img, tipX, c, static_cast<int>(3.0f * u), rgba(255, 250, 210, 255));
    }
}

void drawTarpitTower(Image& img, float u, const Palette& p, int level) {
    const int c = static_cast<int>(32 * u);

    // Sunken stone basin rim
    const int pitR = static_cast<int>((19.0f + level * 1.5f) * u); // Lv1: 20.5u, Lv3: 23.5u
    disc(img, c, c, pitR + static_cast<int>(2.0f * u), rgba(20, 20, 25, 160)); // ground depression
    outlinedDisc(img, c, c, pitR, p.base, p.dark);

    // Dark bubbling tar pool
    const int tarR = pitR - static_cast<int>(3.0f * u);
    disc(img, c, c, tarR, rgba(28, 24, 20, 255));
    disc(img, c, c, tarR - static_cast<int>(2.0f * u), rgba(42, 36, 30, 255));

    // Decaying wooden perimeter stakes / hazard markers
    const int nStakes = 6 + level * 2;
    for (int i = 0; i < nStakes; ++i) {
        const float a = i * (6.2831853f / nStakes);
        const int sx = c + static_cast<int>(std::cos(a) * (pitR - 1.5f * u));
        const int sy = c + static_cast<int>(std::sin(a) * (pitR - 1.5f * u));
        disc(img, sx, sy, static_cast<int>(1.8f * u), p.dark);
        disc(img, sx, sy, std::max(1, static_cast<int>(1.0f * u)), p.base);
    }

    // Viscous swirling ripples and popping tar bubbles
    disc(img, c - static_cast<int>(6 * u), c - static_cast<int>(4 * u), static_cast<int>(3.5f * u), rgba(58, 50, 40, 255));
    disc(img, c + static_cast<int>(5 * u), c + static_cast<int>(5 * u), static_cast<int>(3.0f * u), rgba(58, 50, 40, 255));
    disc(img, c - static_cast<int>(5 * u), c - static_cast<int>(3 * u), static_cast<int>(1.8f * u), p.accent);
    disc(img, c + static_cast<int>(6 * u), c + static_cast<int>(6 * u), static_cast<int>(1.5f * u), p.accent);

    // Level 3 gets an abyssal necrotic green vortex swirl
    if (level >= 3) {
        for (int a = 0; a < 360; a += 60) {
            const float rad = a * 0.0174533f;
            const int vx = c + static_cast<int>(std::cos(rad) * (tarR * 0.55f));
            const int vy = c + static_cast<int>(std::sin(rad) * (tarR * 0.55f));
            disc(img, vx, vy, static_cast<int>(2.2f * u), rgba(120, 200, 80, 200));
        }
        disc(img, c, c, static_cast<int>(4.0f * u), rgba(15, 15, 18, 255)); // vortex eye
    }
}

void drawBeaconTower(Image& img, float u, const Palette& p, int level) {
    const int c = static_cast<int>(32 * u);
    drawBasePlatform(img, u, p, level);

    // Ceremonial sunburst dais with engraved radiating sunbeams
    const int rSun = static_cast<int>((14.0f + level * 1.5f) * u);
    for (int a = 0; a < 360; a += 30) {
        const float rad = a * 0.0174533f;
        const int sx = c + static_cast<int>(std::cos(rad) * rSun);
        const int sy = c + static_cast<int>(std::sin(rad) * rSun);
        line(img, c, c, sx, sy, p.light);
    }

    // Sacred golden brazier ring
    const int rBrazier = static_cast<int>((9.0f + level * 1.0f) * u);
    outlinedDisc(img, c, c, rBrazier, p.base, p.dark);

    // 4 cardinal heraldic banners / eagle standards
    const int nStandards = 4;
    for (int i = 0; i < nStandards; ++i) {
        const float a = i * 1.570796f + 0.785398f;
        const int stx = c + static_cast<int>(std::cos(a) * (rSun - 2.0f * u));
        const int sty = c + static_cast<int>(std::sin(a) * (rSun - 2.0f * u));
        disc(img, stx, sty, static_cast<int>(2.0f * u), p.dark);
        disc(img, stx, sty, std::max(1, static_cast<int>(1.2f * u)), rgba(255, 230, 120, 255));
    }

    // Central blazing sun flame / motivating solar beacon
    const int flareR = static_cast<int>((5.5f + level * 1.2f) * u);
    disc(img, c, c, flareR + static_cast<int>(3.0f * u), rgba(255, 235, 140, 140)); // radiant halo
    disc(img, c, c, flareR, p.accent);
    disc(img, c, c, static_cast<int>(flareR * 0.6f), rgba(255, 255, 230, 255)); // white hot center

    // Level 3 gets an orbiting celestial gold halo ring
    if (level >= 3) {
        for (int a = 0; a < 360; a += 45) {
            const float rad = a * 0.0174533f;
            const int hx = c + static_cast<int>(std::cos(rad) * (flareR + 2.5f * u));
            const int hy = c + static_cast<int>(std::sin(rad) * (flareR + 2.5f * u));
            disc(img, hx, hy, std::max(1, static_cast<int>(1.0f * u)), rgba(255, 245, 180, 255));
        }
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

    // Downsample from 4x supersampled resolution to final size with bilinear filtering
    ImageResize(&img, size, size);
    return img;
}

}  // namespace tf
