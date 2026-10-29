// towerfall — procedural enemy and icon sprites (top-view).
//
// Procedurally generated top-down sprites: zero compute credits,
// zero external image files, mathematically bounded so sprites
// NEVER overflow the 64x64 canvas.
//
// All enemies are drawn facing EAST (0 degrees / +X) so they
// can rotate seamlessly along their movement vector in real time.

#include "render/Sprites.h"

#include <algorithm>
#include <cmath>

namespace tf {
namespace {

constexpr int kSupersample = 4;
constexpr float kLogical = 64.0f;

Color rgba(int r, int g, int b, int a = 255) {
    return Color{static_cast<unsigned char>(r), static_cast<unsigned char>(g),
                 static_cast<unsigned char>(b), static_cast<unsigned char>(a)};
}

struct Palette {
    Color base;
    Color dark;
    Color light;
    Color accent;
};

Palette paletteForEnemy(const std::string& colorName) {
    if (colorName == "red")        return {rgba(205, 62, 56),  rgba(118, 28, 26),  rgba(240, 112, 98),  rgba(255, 226, 175)};
    if (colorName == "orange")     return {rgba(230, 130, 44), rgba(142, 68, 20),  rgba(250, 178, 92),  rgba(255, 238, 192)};
    if (colorName == "brown")      return {rgba(148, 96, 58),  rgba(84, 52, 28),   rgba(195, 144, 96),  rgba(242, 216, 170)};
    if (colorName == "grey")       return {rgba(124, 132, 144),rgba(68, 74, 84),   rgba(178, 186, 198), rgba(218, 228, 240)};
    if (colorName == "violet")     return {rgba(152, 94, 200), rgba(84, 48, 130),  rgba(202, 154, 245), rgba(235, 215, 255)};
    if (colorName == "yellow")     return {rgba(230, 198, 58), rgba(138, 114, 18), rgba(252, 234, 126), rgba(255, 250, 212)};
    if (colorName == "green")      return {rgba(94, 174, 82),  rgba(46, 98, 42),   rgba(152, 216, 126), rgba(216, 250, 192)};
    if (colorName == "darkgreen")  return {rgba(60, 124, 64),  rgba(26, 66, 32),   rgba(110, 174, 104), rgba(192, 238, 178)};
    if (colorName == "purple")     return {rgba(110, 44, 130), rgba(56, 18, 72),   rgba(178, 94, 208),  rgba(242, 208, 255)};
    if (colorName == "crimson")    return {rgba(160, 28, 42),  rgba(82, 12, 22),   rgba(215, 60, 75),   rgba(255, 180, 188)};
    return {rgba(180, 90, 90), rgba(110, 50, 50), rgba(220, 140, 140), rgba(250, 230, 200)};
}

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

// ---------------------------------------------------------------- shapes

// Grunt / baseline walker: streamlined bio-crawler facing East (+X)
void drawBlob(Image& img, float u, const Palette& p) {
    const int c = static_cast<int>(32 * u);

    // Drop shadow
    disc(img, c, c, static_cast<int>(18 * u), rgba(15, 15, 20, 120));

    // Segmented body plates along spine from back to front
    for (int i = 0; i < 4; ++i) {
        const int sx = c - static_cast<int>((12 - i * 6) * u);
        const int r = static_cast<int>((10 + i * 1.5f) * u);
        outlinedDisc(img, sx, c, r, p.base, p.dark);
    }

    // Main head lobe at front (+X)
    const int headX = c + static_cast<int>(8 * u);
    outlinedDisc(img, headX, c, static_cast<int>(10.5f * u), p.light, p.dark);

    // Twin sensory antennae pointing forward
    line(img, headX + static_cast<int>(6 * u), c - static_cast<int>(4 * u),
         headX + static_cast<int>(13 * u), c - static_cast<int>(8 * u), p.dark);
    line(img, headX + static_cast<int>(6 * u), c + static_cast<int>(4 * u),
         headX + static_cast<int>(13 * u), c + static_cast<int>(8 * u), p.dark);
    disc(img, headX + static_cast<int>(13 * u), c - static_cast<int>(8 * u), static_cast<int>(1.5f * u), p.accent);
    disc(img, headX + static_cast<int>(13 * u), c + static_cast<int>(8 * u), static_cast<int>(1.5f * u), p.accent);

    // Glowing eyes facing East
    const int eyeX = headX + static_cast<int>(3 * u);
    disc(img, eyeX, c - static_cast<int>(4 * u), static_cast<int>(2.5f * u), p.accent);
    disc(img, eyeX, c + static_cast<int>(4 * u), static_cast<int>(2.5f * u), p.accent);
    disc(img, eyeX + static_cast<int>(1 * u), c - static_cast<int>(4 * u), static_cast<int>(1.2f * u), rgba(20, 20, 25, 255));
    disc(img, eyeX + static_cast<int>(1 * u), c + static_cast<int>(4 * u), static_cast<int>(1.2f * u), rgba(20, 20, 25, 255));
}

// Runner: supersonic aerodynamic dart / skimmer facing East (+X)
void drawSprite(Image& img, float u, const Palette& p) {
    const int c = static_cast<int>(32 * u);

    // Rear twin thruster exhaust trails
    const int tailX = c - static_cast<int>(13 * u);
    for (int offY : {-static_cast<int>(4 * u), static_cast<int>(4 * u)}) {
        disc(img, tailX - static_cast<int>(5 * u), c + offY, static_cast<int>(2.5f * u), rgba(255, 140, 40, 220));
        disc(img, tailX - static_cast<int>(2 * u), c + offY, static_cast<int>(3.5f * u), rgba(255, 220, 80, 255));
    }

    // Swept delta-wings (drawing layered polygons/lines)
    const int noseX = c + static_cast<int>(21 * u);
    const int wingX = c - static_cast<int>(11 * u);
    const int wingSpan = static_cast<int>(16 * u);

    for (int w = 0; w <= wingSpan; ++w) {
        const float t = static_cast<float>(w) / wingSpan;
        const int fx = static_cast<int>(noseX - t * (noseX - wingX));
        line(img, fx, c - w, wingX, c - w, p.dark);
        line(img, fx, c + w, wingX, c + w, p.dark);
        if (w > 2) {
            line(img, fx + static_cast<int>(1 * u), c - w + 1, wingX - static_cast<int>(1 * u), c - w + 1, p.base);
            line(img, fx + static_cast<int>(1 * u), c + w - 1, wingX - static_cast<int>(1 * u), c + w - 1, p.base);
        }
    }

    // Central fuselage spine
    rect(img, c - static_cast<int>(10 * u), c - static_cast<int>(3.5f * u),
         static_cast<int>(24 * u), static_cast<int>(7.0f * u), p.light);

    // Sleek cockpit canopy & forward sensor visor
    disc(img, c + static_cast<int>(6 * u), c, static_cast<int>(4.2f * u), p.accent);
    disc(img, c + static_cast<int>(8 * u), c, static_cast<int>(2.2f * u), rgba(255, 255, 255, 255));

    // Wingtip stabilizer fins
    disc(img, wingX, c - wingSpan, static_cast<int>(2.2f * u), p.accent);
    disc(img, wingX, c + wingSpan, static_cast<int>(2.2f * u), p.accent);
}

// Crawler: armored 6-legged insectoid beetle facing East (+X)
void drawBug(Image& img, float u, const Palette& p) {
    const int c = static_cast<int>(32 * u);

    // 6 articulated jointed legs (3 on each side)
    const int legSpan = static_cast<int>(19 * u);
    for (int side : {-1, 1}) {
        // Front leg (forward-swept)
        line(img, c + static_cast<int>(4 * u), c + side * static_cast<int>(5 * u),
             c + static_cast<int>(12 * u), c + side * (legSpan - static_cast<int>(4 * u)), p.dark);
        line(img, c + static_cast<int>(12 * u), c + side * (legSpan - static_cast<int>(4 * u)),
             c + static_cast<int>(18 * u), c + side * legSpan, p.dark);

        // Middle leg (outward)
        line(img, c - static_cast<int>(2 * u), c + side * static_cast<int>(6 * u),
             c - static_cast<int>(1 * u), c + side * (legSpan - static_cast<int>(2 * u)), p.dark);
        line(img, c - static_cast<int>(1 * u), c + side * (legSpan - static_cast<int>(2 * u)),
             c + static_cast<int>(1 * u), c + side * legSpan, p.dark);

        // Rear leg (backward-swept)
        line(img, c - static_cast<int>(8 * u), c + side * static_cast<int>(5 * u),
             c - static_cast<int>(14 * u), c + side * (legSpan - static_cast<int>(3 * u)), p.dark);
        line(img, c - static_cast<int>(14 * u), c + side * (legSpan - static_cast<int>(3 * u)),
             c - static_cast<int>(18 * u), c + side * legSpan, p.dark);
    }

    // Segmented abdomen & armored elytra (wing-covers)
    const int abdX = c - static_cast<int>(6 * u);
    const int abdR = static_cast<int>(12.5f * u);
    outlinedDisc(img, abdX, c, abdR, p.base, p.dark);

    // Elytra central suture seam
    line(img, abdX - abdR + static_cast<int>(1 * u), c, abdX + abdR, c, p.dark);

    // Armored defensive spots / chitin nodes
    disc(img, abdX - static_cast<int>(3 * u), c - static_cast<int>(5 * u), static_cast<int>(2.2f * u), p.accent);
    disc(img, abdX - static_cast<int>(3 * u), c + static_cast<int>(5 * u), static_cast<int>(2.2f * u), p.accent);
    disc(img, abdX + static_cast<int>(3 * u), c - static_cast<int>(4 * u), static_cast<int>(1.8f * u), p.accent);
    disc(img, abdX + static_cast<int>(3 * u), c + static_cast<int>(4 * u), static_cast<int>(1.8f * u), p.accent);

    // Thorax & Head
    const int headX = c + static_cast<int>(8 * u);
    outlinedDisc(img, headX, c, static_cast<int>(8.0f * u), p.dark, p.base);

    // Forward pinching mandibles
    line(img, headX + static_cast<int>(5 * u), c - static_cast<int>(3 * u),
         headX + static_cast<int>(12 * u), c - static_cast<int>(6 * u), p.dark);
    line(img, headX + static_cast<int>(12 * u), c - static_cast<int>(6 * u),
         headX + static_cast<int>(14 * u), c - static_cast<int>(1 * u), p.dark);

    line(img, headX + static_cast<int>(5 * u), c + static_cast<int>(3 * u),
         headX + static_cast<int>(12 * u), c + static_cast<int>(6 * u), p.dark);
    line(img, headX + static_cast<int>(12 * u), c + static_cast<int>(6 * u),
         headX + static_cast<int>(14 * u), c + static_cast<int>(1 * u), p.dark);

    // Compound eyes
    disc(img, headX + static_cast<int>(4 * u), c - static_cast<int>(5 * u), static_cast<int>(2.2f * u), p.accent);
    disc(img, headX + static_cast<int>(4 * u), c + static_cast<int>(5 * u), static_cast<int>(2.2f * u), p.accent);
}

// Wraith: ethereal specter facing East (+X) with trailing spirit wisps
void drawGhost(Image& img, float u, const Palette& p) {
    const int c = static_cast<int>(32 * u);

    // Ethereal soul aura / shadow haze
    disc(img, c - static_cast<int>(2 * u), c, static_cast<int>(19 * u), rgba(120, 60, 180, 80));

    // Trailing ghostly tendrils streaming backwards (-X)
    for (int yOff : {-static_cast<int>(8 * u), -static_cast<int>(3 * u), static_cast<int>(3 * u), static_cast<int>(8 * u)}) {
        for (int i = 0; i < 8; ++i) {
            const int tx = c - static_cast<int>(i * 2.3f * u);
            const int ty = c + yOff + static_cast<int>(std::sin(i * 0.8f) * 2.5f * u);
            disc(img, tx, ty, static_cast<int>((5.0f - i * 0.5f) * u), p.dark);
        }
    }

    // Main ghostly shroud body
    const int shroudX = c - static_cast<int>(1 * u);
    disc(img, shroudX, c, static_cast<int>(14.0f * u), p.base);
    disc(img, shroudX, c, static_cast<int>(11.0f * u), p.light);

    // Forward spectral skull mask (+X)
    const int headX = c + static_cast<int>(7 * u);
    disc(img, headX, c, static_cast<int>(9.0f * u), p.base);
    disc(img, headX, c, static_cast<int>(7.0f * u), p.light);

    // Hollow glowing hypnotic eyes
    const int eyeX = headX + static_cast<int>(3 * u);
    disc(img, eyeX, c - static_cast<int>(3.8f * u), static_cast<int>(2.5f * u), rgba(30, 15, 45, 255));
    disc(img, eyeX, c + static_cast<int>(3.8f * u), static_cast<int>(2.5f * u), rgba(30, 15, 45, 255));
    disc(img, eyeX, c - static_cast<int>(3.8f * u), static_cast<int>(1.2f * u), p.accent);
    disc(img, eyeX, c + static_cast<int>(3.8f * u), static_cast<int>(1.2f * u), p.accent);

    // Mystical third eye rune on forehead
    disc(img, headX + static_cast<int>(6 * u), c, static_cast<int>(1.5f * u), p.accent);
}

// Flyer / Wasp: top-down winged predator facing East (+X)
void drawWing(Image& img, float u, const Palette& p) {
    const int c = static_cast<int>(32 * u);

    // Outstretched translucent wings spreading along Y axis
    const int wingSpan = static_cast<int>(20 * u);
    for (int side : {-1, 1}) {
        const int wy = c + side * wingSpan;
        // Wing membranes
        for (int step = 0; step < 7; ++step) {
            const int mx = c - static_cast<int>(6 * u) + static_cast<int>(step * 2.2f * u);
            const int my = c + side * static_cast<int>((5 + step * 2.3f) * u);
            disc(img, mx, my, static_cast<int>(5.5f * u), rgba(210, 235, 255, 80));
            disc(img, mx, my, static_cast<int>(3.5f * u), rgba(235, 245, 255, 120));
        }
        // Main structural wing veins
        line(img, c, c + side * static_cast<int>(4 * u), c + static_cast<int>(4 * u), wy, rgba(240, 250, 255, 220));
        line(img, c, c + side * static_cast<int>(4 * u), c - static_cast<int>(6 * u), wy - side * static_cast<int>(3 * u), rgba(240, 250, 255, 180));
    }

    // Striped abdomen tapering to barbed stinger at rear (-X)
    for (int i = 0; i < 5; ++i) {
        const int ax = c - static_cast<int>((4 + i * 3) * u);
        const int ar = static_cast<int>((7.5f - i * 1.1f) * u);
        disc(img, ax, c, ar, (i % 2 == 0) ? p.dark : p.accent);
    }
    // Barbed stinger at tip
    line(img, c - static_cast<int>(16 * u), c, c - static_cast<int>(20 * u), c, p.dark);

    // Thorax at center
    outlinedDisc(img, c + static_cast<int>(2 * u), c, static_cast<int>(7.0f * u), p.base, p.dark);

    // Head with compound eyes & antennae at front (+X)
    const int headX = c + static_cast<int>(11 * u);
    outlinedDisc(img, headX, c, static_cast<int>(5.5f * u), p.light, p.dark);

    // Compound eyes
    disc(img, headX + static_cast<int>(2 * u), c - static_cast<int>(3.5f * u), static_cast<int>(2.2f * u), p.accent);
    disc(img, headX + static_cast<int>(2 * u), c + static_cast<int>(3.5f * u), static_cast<int>(2.2f * u), p.accent);

    // Antennae
    line(img, headX + static_cast<int>(4 * u), c - static_cast<int>(2 * u),
         headX + static_cast<int>(10 * u), c - static_cast<int>(5 * u), p.dark);
    line(img, headX + static_cast<int>(4 * u), c + static_cast<int>(2 * u),
         headX + static_cast<int>(10 * u), c + static_cast<int>(5 * u), p.dark);
}

// Sapper & Bruiser: mechanized siege combat automaton facing East (+X)
void drawHex(Image& img, float u, const Palette& p) {
    const int c = static_cast<int>(32 * u);

    // Side tread / walker leg sponsons
    rect(img, c - static_cast<int>(11 * u), c - static_cast<int>(14 * u),
         static_cast<int>(20 * u), static_cast<int>(4 * u), p.dark);
    rect(img, c - static_cast<int>(11 * u), c + static_cast<int>(10 * u),
         static_cast<int>(20 * u), static_cast<int>(4 * u), p.dark);

    // Angular hexagonal heavy hull
    const int hR = static_cast<int>(13.5f * u);
    outlinedDisc(img, c, c, hR, p.base, p.dark);

    // Hazard warning chevron stripes across back
    for (int offX = -8; offX <= 2; offX += 4) {
        const int sx = c + static_cast<int>(offX * u);
        line(img, sx, c - static_cast<int>(7 * u), sx + static_cast<int>(3 * u), c + static_cast<int>(7 * u), p.accent);
    }

    // Rear dual exhaust stacks with dark pipe bores
    disc(img, c - static_cast<int>(11 * u), c - static_cast<int>(6 * u), static_cast<int>(2.8f * u), p.dark);
    disc(img, c - static_cast<int>(11 * u), c + static_cast<int>(6 * u), static_cast<int>(2.8f * u), p.dark);
    disc(img, c - static_cast<int>(11 * u), c - static_cast<int>(6 * u), static_cast<int>(1.4f * u), rgba(15, 15, 20, 255));
    disc(img, c - static_cast<int>(11 * u), c + static_cast<int>(6 * u), static_cast<int>(1.4f * u), rgba(15, 15, 20, 255));

    // Heavy reinforced front bumper / ram
    rect(img, c + static_cast<int>(8 * u), c - static_cast<int>(9 * u),
         static_cast<int>(4 * u), static_cast<int>(18 * u), p.dark);

    // Forward hydraulic crusher jaws / rotary drill bit (+X)
    const int drillTipX = c + static_cast<int>(21 * u);
    line(img, c + static_cast<int>(12 * u), c - static_cast<int>(6 * u), drillTipX, c, p.accent);
    line(img, c + static_cast<int>(12 * u), c + static_cast<int>(6 * u), drillTipX, c, p.accent);
    disc(img, drillTipX, c, static_cast<int>(2.2f * u), p.accent);

    // Armored central cupola / optical sensor visor
    outlinedDisc(img, c + static_cast<int>(2 * u), c, static_cast<int>(5.0f * u), p.light, p.dark);
    disc(img, c + static_cast<int>(4 * u), c, static_cast<int>(2.0f * u), rgba(255, 60, 40, 255)); // red sensor
}

// Colossus & Ravager: colossal apex bosses facing East (+X)
void drawMonster(Image& img, float u, const Palette& p) {
    const int c = static_cast<int>(32 * u);

    // Heavy drop shadow
    disc(img, c, c, static_cast<int>(22 * u), rgba(10, 10, 15, 140));

    // Massive titan armored carapace
    const int bodyR = static_cast<int>(16.5f * u);
    outlinedDisc(img, c - static_cast<int>(2 * u), c, bodyR, p.base, p.dark);

    // Spiked heavy shoulder pauldrons
    disc(img, c - static_cast<int>(2 * u), c - static_cast<int>(15 * u), static_cast<int>(5.5f * u), p.dark);
    disc(img, c - static_cast<int>(2 * u), c + static_cast<int>(15 * u), static_cast<int>(5.5f * u), p.dark);
    disc(img, c - static_cast<int>(2 * u), c - static_cast<int>(15 * u), static_cast<int>(3.5f * u), p.accent);
    disc(img, c - static_cast<int>(2 * u), c + static_cast<int>(15 * u), static_cast<int>(3.5f * u), p.accent);

    // Carapace dorsal armor ridge & ancient runic inlays
    for (int i = -1; i <= 2; ++i) {
        const int rx = c - static_cast<int>((6 - i * 4) * u);
        disc(img, rx, c, static_cast<int>((6.0f - std::abs(i) * 0.8f) * u), p.dark);
        disc(img, rx, c, static_cast<int>((4.0f - std::abs(i) * 0.8f) * u), p.light);
    }

    // Colossal forward sweeping horns / curved scythe tusks reaching East (+X)
    const int tuskTipX = c + static_cast<int>(23 * u);
    for (int side : {-1, 1}) {
        const int baseY = c + side * static_cast<int>(10 * u);
        const int midY = c + side * static_cast<int>(14 * u);
        const int tipY = c + side * static_cast<int>(4 * u);

        line(img, c + static_cast<int>(6 * u), baseY, c + static_cast<int>(16 * u), midY, p.dark);
        line(img, c + static_cast<int>(16 * u), midY, tuskTipX, tipY, p.dark);
        disc(img, tuskTipX, tipY, static_cast<int>(2.2f * u), p.accent);
    }

    // Heavy fortified skull plate & glowing central eye
    const int headX = c + static_cast<int>(10 * u);
    outlinedDisc(img, headX, c, static_cast<int>(7.5f * u), p.dark, p.base);

    // Blazing cyclopean core eye
    disc(img, headX + static_cast<int>(2 * u), c, static_cast<int>(3.5f * u), rgba(255, 230, 80, 255));
    disc(img, headX + static_cast<int>(3 * u), c, static_cast<int>(1.8f * u), rgba(220, 40, 30, 255));
    disc(img, headX + static_cast<int>(3.5f * u), c, static_cast<int>(0.8f * u), rgba(255, 255, 255, 255));
}

}  // namespace

Image renderEnemySprite(EnemyKind kind, const std::string& shape,
                        const std::string& color, float scale, int size) {
    const int big = size * kSupersample;
    Image img = GenImageColor(big, big, Color{0, 0, 0, 0});
    const Palette p = paletteForEnemy(color);
    const float u = big / kLogical;

    if (shape == "sprite")       drawSprite(img, u, p);
    else if (shape == "bug")     drawBug(img, u, p);
    else if (shape == "ghost")   drawGhost(img, u, p);
    else if (shape == "wing")    drawWing(img, u, p);
    else if (shape == "hex")     drawHex(img, u, p);
    else if (shape == "monster") drawMonster(img, u, p);
    else                         drawBlob(img, u, p);

    // Bosses receive a magnificent golden royal crown at the front (+X)
    if (isBoss(kind)) {
        const int c = static_cast<int>(32 * u);
        const int crownX = c + static_cast<int>(16 * u);

        // Gold band
        rect(img, crownX - static_cast<int>(1 * u), c - static_cast<int>(7 * u),
             static_cast<int>(3 * u), static_cast<int>(14 * u), rgba(255, 215, 60, 255));

        // 3 Crown spires pointing forward (+X)
        for (int offY : {-static_cast<int>(5 * u), 0, static_cast<int>(5 * u)}) {
            const int spireLen = (offY == 0) ? static_cast<int>(6 * u) : static_cast<int>(4 * u);
            line(img, crownX, c + offY, crownX + spireLen, c + offY, rgba(255, 215, 60, 255));
            disc(img, crownX + spireLen, c + offY, static_cast<int>(1.6f * u), rgba(255, 240, 150, 255));
        }
        // Center ruby jewel
        disc(img, crownX + static_cast<int>(1 * u), c, static_cast<int>(1.8f * u), rgba(220, 30, 40, 255));
    }

    // Downsample from 4x supersampled resolution with smooth bilinear filtering
    ImageResize(&img, size, size);
    return img;
}

// ---------------------------------------------------------------- icons

Image renderHeartSprite(int size) {
    const int big = size * kSupersample;
    Image img = GenImageColor(big, big, Color{0, 0, 0, 0});
    const int c = big / 2;
    const int r = static_cast<int>(big * 0.25f);

    disc(img, c - r / 2, c - r / 4, r, rgba(226, 62, 62, 255));
    disc(img, c + r / 2, c - r / 4, r, rgba(226, 62, 62, 255));

    for (int y = -r / 4; y <= r; ++y) {
        const float t = (y + r / 4) / (r * 1.25f);
        const int half = static_cast<int>(r * 2.0f * std::sqrt(std::max(0.0f, 1.0f - t * t)));
        if (half <= 0) continue;
        ImageDrawRectangle(&img, c - half, c - r / 4 + y, half * 2, 1, rgba(226, 62, 62, 255));
    }
    disc(img, c - r / 3, c - r / 3, r / 4, rgba(255, 170, 170, 200));

    ImageResize(&img, size, size);
    return img;
}

Image renderCoinSprite(int size) {
    const int big = size * kSupersample;
    Image img = GenImageColor(big, big, Color{0, 0, 0, 0});
    const int c = big / 2;

    disc(img, c, c, static_cast<int>(big * 0.38f), rgba(198, 158, 44, 255));
    outlinedDisc(img, c, c, static_cast<int>(big * 0.30f),
                 rgba(246, 208, 84, 255), rgba(168, 128, 32, 255));
    ImageDrawRectangle(&img, c - 2, c - static_cast<int>(big * 0.16f), 4, static_cast<int>(big * 0.32f), rgba(168, 128, 32, 255));

    ImageResize(&img, size, size);
    return img;
}

Image renderSkullSprite(int size) {
    const int big = size * kSupersample;
    Image img = GenImageColor(big, big, Color{0, 0, 0, 0});
    const int c = big / 2;

    disc(img, c, c - static_cast<int>(big * 0.05f), static_cast<int>(big * 0.32f), rgba(238, 238, 232, 255));
    ImageDrawRectangle(&img, c - static_cast<int>(big * 0.16f), c + static_cast<int>(big * 0.12f),
                       static_cast<int>(big * 0.32f), static_cast<int>(big * 0.16f), rgba(238, 238, 232, 255));
    disc(img, c - static_cast<int>(big * 0.12f), c - static_cast<int>(big * 0.06f), static_cast<int>(big * 0.10f), rgba(30, 30, 34, 255));
    disc(img, c + static_cast<int>(big * 0.12f), c - static_cast<int>(big * 0.06f), static_cast<int>(big * 0.10f), rgba(30, 30, 34, 255));
    for (int i = -1; i <= 1; ++i) {
        ImageDrawRectangle(&img, c + static_cast<int>(i * big * 0.09f) - 1, c + static_cast<int>(big * 0.14f),
                           2, static_cast<int>(big * 0.12f), rgba(30, 30, 34, 255));
    }

    ImageResize(&img, size, size);
    return img;
}

Image renderCoreSprite(int size) {
    const int big = size * kSupersample;
    Image img = GenImageColor(big, big, Color{0, 0, 0, 0});
    const int c = big / 2;
    const int r = static_cast<int>(big * 0.34f);

    for (int y = -r; y <= r; ++y) {
        const int half = static_cast<int>(r * std::sqrt(std::max(0.0f, 1.0f - (y * y) / (float)(r * r))));
        ImageDrawRectangle(&img, c - half, c + y, half * 2, 1,
                           y < 0 ? rgba(140, 226, 255, 255) : rgba(60, 140, 210, 255));
    }
    for (int i = 0; i < 3; ++i) {
        ImageDrawCircle(&img, c, c, r + 4 - i, Color{120, 210, 255, 30});
    }

    ImageResize(&img, size, size);
    return img;
}

Image renderTowerIcon(TowerKind kind, int size) {
    return renderTowerSprite(kind, 1, size);
}

}  // namespace tf
