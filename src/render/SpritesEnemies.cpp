// towerfall — procedural enemy and icon sprites.
//
// Same idea as the tower sprites in Sprites.cpp: everything is drawn, nothing
// is loaded. Enemy bodies are distinguished by SHAPE as well as colour, so the
// player can tell a Sapper from a Grunt at a glance, even in a crowded wave:
//   blob     - the baseline grunt
//   sprite   - a fast runner, with a pointed crown
//   bug      - a slow armoured crawler, with legs
//   ghost    - a wraith with a scalloped hem (and it ignores slows)
//   wing     - a flyer, drawn with beating wings
//   hex      - a tower-attacker, angular and spiky
//   monster  - a boss: hulking, spiked, with one big glowing eye

#include "render/Sprites.h"

#include <cmath>

namespace tf {
namespace {

constexpr int kSupersample = 4;

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
    if (colorName == "red")        return {rgba(198, 66, 60),  rgba(122, 32, 30),  rgba(236, 110, 96),  rgba(255, 226, 176)};
    if (colorName == "orange")     return {rgba(226, 132, 48), rgba(146, 74, 22),  rgba(248, 178, 96),  rgba(255, 236, 190)};
    if (colorName == "brown")      return {rgba(146, 98, 60),  rgba(88, 56, 32),   rgba(192, 142, 96),  rgba(240, 214, 168)};
    if (colorName == "grey")       return {rgba(122, 130, 140),rgba(70, 76, 86),   rgba(174, 184, 196), rgba(214, 226, 238)};
    if (colorName == "violet")     return {rgba(150, 96, 196), rgba(86, 50, 132),  rgba(198, 152, 240), rgba(232, 212, 255)};
    if (colorName == "yellow")     return {rgba(226, 200, 62), rgba(140, 118, 20), rgba(250, 234, 130), rgba(255, 250, 210)};
    if (colorName == "green")      return {rgba(96, 172, 84),  rgba(48, 100, 44),  rgba(150, 214, 128), rgba(214, 250, 190)};
    if (colorName == "darkgreen")  return {rgba(62, 126, 66),  rgba(28, 68, 34),   rgba(112, 172, 106), rgba(190, 236, 176)};
    if (colorName == "purple")     return {rgba(112, 48, 132), rgba(58, 20, 74),   rgba(176, 96, 206), rgba(240, 206, 255)};
    if (colorName == "crimson")    return {rgba(158, 30, 44),  rgba(84, 14, 24),   rgba(212, 62, 76),  rgba(255, 178, 186)};
    return {rgba(180, 90, 90), rgba(110, 50, 50), rgba(220, 140, 140), rgba(250, 230, 200)};
}

void disc(Image& img, int cx, int cy, int r, Color c) {
    if (r <= 0) return;
    ImageDrawCircle(&img, cx, cy, r, c);
}

// An outline is drawn by painting the shape in the outline colour, then the fill
// on top. Punching a transparent hole does NOT work: raylib blends, so the
// "hole" either does nothing or erases the fill as well.
void outlinedDisc(Image& img, int cx, int cy, int r, Color fill, Color outline) {
    disc(img, cx, cy, r + 1, outline);
    disc(img, cx, cy, r, fill);
}

// ---------------------------------------------------------------- shapes

void drawBlob(Image& img, float u, const Palette& p) {
    const int c = static_cast<int>(32 * u);
    const int r = static_cast<int>(24 * u);
    // The outline and body go FIRST: outlinedDisc() repaints the whole disc,
    // so calling it last would erase the belly and the eyes.
    outlinedDisc(img, c, c, r, p.base, p.dark);
    disc(img, c, c + r / 3, std::max(1, static_cast<int>(r * 0.6f)), p.light);
    disc(img, c - r / 3, c - r / 4, std::max(1, static_cast<int>(r / 5.0f)), p.accent);
    disc(img, c + r / 3, c - r / 4, std::max(1, static_cast<int>(r / 5.0f)), p.accent);
    disc(img, c - r / 3, c - r / 4, std::max(1, static_cast<int>(r / 10.0f)), Color{20, 20, 20, 255});
    disc(img, c + r / 3, c - r / 4, std::max(1, static_cast<int>(r / 10.0f)), Color{20, 20, 20, 255});
}

void drawSprite(Image& img, float u, const Palette& p) {
    const int c = static_cast<int>(32 * u);
    const int r = static_cast<int>(22 * u);
    for (int y = -r; y <= r; ++y) {
        const float t = static_cast<float>(y) / r;
        const int half = static_cast<int>(r * std::sqrt(std::max(0.0f, 1.0f - t * t * 0.85f)));
        ImageDrawRectangle(&img, c - half, c + y, half * 2, 1, y < 0 ? p.light : p.base);
    }
    outlinedDisc(img, c, c, r, p.base, p.dark);
    for (int i = 0; i < 3; ++i) {
        const int ox = (i - 1) * static_cast<int>(5 * u);
        for (int y = 0; y < 6; ++y) {
            ImageDrawRectangle(&img, c + ox - static_cast<int>((6 - y) * u),
                                c - r - static_cast<int>(y * u),
                                static_cast<int>((6 - y) * 2 * u),
                                std::max(1, static_cast<int>(u)), p.accent);
        }
    }
    disc(img, c - r / 3, c - r / 4, std::max(1, static_cast<int>(r / 6.0f)), Color{20, 20, 20, 255});
    disc(img, c + r / 3, c - r / 4, std::max(1, static_cast<int>(r / 6.0f)), Color{20, 20, 20, 255});
}

void drawBug(Image& img, float u, const Palette& p) {
    const int c = static_cast<int>(32 * u);
    const int r = static_cast<int>(21 * u);
    for (int i = -2; i <= 2; ++i) {
        const int y = c + i * static_cast<int>(4 * u);
        ImageDrawLine(&img, c - r, y, c - r - static_cast<int>(5 * u),
                      y + static_cast<int>(3 * u), p.dark);
        ImageDrawLine(&img, c + r, y, c + r + static_cast<int>(5 * u),
                      y + static_cast<int>(3 * u), p.dark);
    }
    outlinedDisc(img, c, c, r, p.base, p.dark);
    ImageDrawRectangle(&img, c - static_cast<int>(2 * u), c - r,
                         static_cast<int>(4 * u), r * 2, p.light);
    disc(img, c, c - r - static_cast<int>(3 * u),
         std::max(1, static_cast<int>(r / 2.0f)), p.dark);
    disc(img, c - static_cast<int>(2 * u), c - r - static_cast<int>(4 * u),
         std::max(1, static_cast<int>(u)), p.accent);
    disc(img, c + static_cast<int>(2 * u), c - r - static_cast<int>(4 * u),
         std::max(1, static_cast<int>(u)), p.accent);
}

void drawGhost(Image& img, float u, const Palette& p) {
    const int c = static_cast<int>(32 * u);
    const int r = static_cast<int>(23 * u);
    for (int y = -r; y <= r; ++y) {
        const float t = static_cast<float>(y) / r;
        int half;
        if (y < 0) {
            half = static_cast<int>(r * std::sqrt(std::max(0.0f, 1.0f - t * t)));
        } else {
            half = r - ((y % static_cast<int>(6 * u) < static_cast<int>(3 * u))
                        ? 0 : static_cast<int>(4 * u));
        }
        ImageDrawRectangle(&img, c - half, c + y, half * 2, 1, p.base);
    }
    disc(img, c - r / 3, c - r / 3, std::max(1, static_cast<int>(r / 6.0f)),
         Color{30, 20, 40, 255});
    disc(img, c + r / 3, c - r / 3, std::max(1, static_cast<int>(r / 6.0f)),
         Color{30, 20, 40, 255});
    ImageDrawRectangle(&img, c - r / 4, c + r / 4, r / 2,
                         std::max(1, static_cast<int>(2 * u)), Color{30, 20, 40, 255});
    for (int i = 0; i < 5; ++i) {
        ImageDrawCircle(&img, c + r - i * static_cast<int>(3 * u),
                          c + r - static_cast<int>(2 * u),
                          std::max(1, static_cast<int>((2 - i / 2.0f) * u)), p.light);
    }
}

void drawWing(Image& img, float u, const Palette& p) {
    const int c = static_cast<int>(32 * u);
    const int r = static_cast<int>(15 * u);
    for (int s = -1; s <= 1; s += 2) {
        for (int i = 0; i < 8; ++i) {
            const int x = c + s * (r + i * static_cast<int>(u));
            const int y = c - static_cast<int>(2 * u) - i * static_cast<int>(2 * u);
            ImageDrawCircle(&img, x, y,
                              std::max(1, static_cast<int>((r - i / 2.0f) * u)), p.light);
        }
    }
    for (int y = -r; y <= r; ++y) {
        const int half = r - static_cast<int>(std::abs(y) * 0.2);
        ImageDrawRectangle(&img, c - half, c + y, half * 2, 1,
                           ((y + r) / 3) % 2 ? p.dark : p.base);
    }
    disc(img, c, c - r, std::max(1, static_cast<int>(r / 2.0f)), p.base);
    disc(img, c - static_cast<int>(2 * u), c - r - static_cast<int>(u),
         std::max(1, static_cast<int>(u)), p.accent);
    disc(img, c + static_cast<int>(2 * u), c - r - static_cast<int>(u),
         std::max(1, static_cast<int>(u)), p.accent);
}

void drawHex(Image& img, float u, const Palette& p) {
    const int c = static_cast<int>(32 * u);
    const int r = static_cast<int>(23 * u);
    for (int y = -r; y <= r; ++y) {
        const int half = r - static_cast<int>(std::abs(y) * 0.5);
        ImageDrawRectangle(&img, c - half, c + y, half * 2, 1, p.base);
    }
    disc(img, c, c, std::max(1, static_cast<int>(r * 0.45f)), p.light);
    outlinedDisc(img, c, c, static_cast<int>(r * 0.5f), p.light, p.dark);
    // The spikes are drawn last so they sit over the body edge.
    disc(img, c - r / 3, c - r / 6, std::max(1, static_cast<int>(r / 7.0f)), p.accent);
    disc(img, c + r / 3, c - r / 6, std::max(1, static_cast<int>(r / 7.0f)), p.accent);
    for (int i = 0; i < 6; ++i) {
        const float a = i * 3.14159f / 3.0f;
        ImageDrawLine(&img, c + static_cast<int>(std::cos(a) * r * 0.5),
                      c + static_cast<int>(std::sin(a) * r * 0.5),
                      c + static_cast<int>(std::cos(a) * r),
                      c + static_cast<int>(std::sin(a) * r), p.dark);
    }
}

void drawMonster(Image& img, float u, const Palette& p) {
    const int c = static_cast<int>(32 * u);
    const int r = static_cast<int>(26 * u);
    disc(img, c, c + r / 4, r, p.base);
    disc(img, c, c + r / 3, std::max(1, static_cast<int>(r * 0.62f)), p.light);
    disc(img, c - static_cast<int>(r * 0.8f), c - static_cast<int>(r * 0.4f),
         std::max(1, static_cast<int>(r * 0.42f)), p.dark);
    disc(img, c + static_cast<int>(r * 0.8f), c - static_cast<int>(r * 0.4f),
         std::max(1, static_cast<int>(r * 0.42f)), p.dark);
    for (int i = -2; i <= 2; ++i) {
        const int x = c + i * r / 3;
        for (int y = 0; y < 7; ++y) {
            ImageDrawRectangle(&img, x - static_cast<int>((7 - y) * u),
                                c - r + static_cast<int>(y * u),
                                static_cast<int>((7 - y) * 2 * u),
                                std::max(1, static_cast<int>(u)), p.accent);
        }
    }
    disc(img, c, c - r / 5, std::max(1, static_cast<int>(r / 3.0f)),
         Color{255, 240, 200, 255});
    disc(img, c, c - r / 5, std::max(1, static_cast<int>(r / 6.0f)),
         Color{190, 40, 30, 255});
    for (int i = -1; i <= 1; ++i) {
        const int x = c + i * r / 3;
        for (int y = 0; y < 5; ++y) {
            ImageDrawRectangle(&img, x - static_cast<int>((5 - y) * u),
                                c + r / 2 + static_cast<int>(y * u),
                                static_cast<int>((5 - y) * 2 * u),
                                std::max(1, static_cast<int>(u)),
                                Color{255, 250, 235, 255});
        }
    }
    outlinedDisc(img, c, c + r / 4, r, p.base, p.dark);
}

}  // namespace

Image renderEnemySprite(EnemyKind kind, const std::string& shape,
                        const std::string& color, float scale, int size) {
    const int big = size * kSupersample;
    Image img = GenImageColor(big, big, Color{0, 0, 0, 0});
    const Palette p = paletteForEnemy(color);

    const float u = big / 64.0f;
    if (shape == "sprite")       drawSprite(img, u, p);
    else if (shape == "bug")      drawBug(img, u, p);
    else if (shape == "ghost")    drawGhost(img, u, p);
    else if (shape == "wing")     drawWing(img, u, p);
    else if (shape == "hex")      drawHex(img, u, p);
    else if (shape == "monster")  drawMonster(img, u, p);
    else                          drawBlob(img, u, p);

    // Bosses get a crown, so they read as bosses at a glance.
    if (isBoss(kind)) {
        const int c = big / 2;
        const int r = static_cast<int>(big * 0.40f);
        const float cu = big / 64.0f;
        for (int i = -1; i <= 1; ++i) {
            const int ox = i * static_cast<int>(7 * cu);
            for (int y = 0; y < 9; ++y) {
                ImageDrawRectangle(&img, c + ox - static_cast<int>((9 - y) * cu),
                                    c - r - static_cast<int>((4 + y) * cu),
                                    static_cast<int>((9 - y) * 2 * cu),
                                    std::max(1, static_cast<int>(cu)), rgba(250, 206, 74, 255));
            }
        }
    }

    (void)scale;
    ImageResizeNN(&img, size, size);
    return img;
}

// ---------------------------------------------------------------- icons

Image renderHeartSprite(int size) {
    const int big = size * kSupersample;
    Image img = GenImageColor(big, big, Color{0, 0, 0, 0});
    const int c = big / 2;
    const int r = big * 0.26;
    disc(img, c - r / 2, c - r / 4, r, rgba(226, 62, 62, 255));
    disc(img, c + r / 2, c - r / 4, r, rgba(226, 62, 62, 255));
    for (int y = -r / 4; y <= r; ++y) {
        const float t = (y + r / 4) / (r * 1.25f);
        const int half = static_cast<int>(r * 2.0f * std::sqrt(std::max(0.0f, 1.0f - t * t)));
        if (half <= 0) continue;
        ImageDrawRectangle(&img, c - half, c - r / 4 + y, half * 2, 1, rgba(226, 62, 62, 255));
    }
    disc(img, c - r / 3, c - r / 3, r / 4, rgba(255, 170, 170, 200));
    ImageResizeNN(&img, size, size);
    return img;
}

Image renderCoinSprite(int size) {
    const int big = size * kSupersample;
    Image img = GenImageColor(big, big, Color{0, 0, 0, 0});
    const int c = big / 2;
    disc(img, c, c, static_cast<int>(big * 0.38f), rgba(198, 158, 44, 255));
    outlinedDisc(img, c, c, static_cast<int>(big * 0.30f),
                  rgba(246, 208, 84, 255), rgba(168, 128, 32, 255));
    ImageDrawRectangle(&img, c - 2, c - big * 0.16, 4, big * 0.32, rgba(168, 128, 32, 255));
    ImageResizeNN(&img, size, size);
    return img;
}

Image renderSkullSprite(int size) {
    const int big = size * kSupersample;
    Image img = GenImageColor(big, big, Color{0, 0, 0, 0});
    const int c = big / 2;
    disc(img, c, c - big * 0.05, big * 0.32, rgba(238, 238, 232, 255));
    ImageDrawRectangle(&img, c - big * 0.16, c + big * 0.12, big * 0.32, big * 0.16,
                       rgba(238, 238, 232, 255));
    disc(img, c - big * 0.12, c - big * 0.06, big * 0.10, rgba(30, 30, 34, 255));
    disc(img, c + big * 0.12, c - big * 0.06, big * 0.10, rgba(30, 30, 34, 255));
    for (int i = -1; i <= 1; ++i) {
        ImageDrawRectangle(&img, c + i * big * 0.09 - 1, c + big * 0.14, 2, big * 0.12,
                           rgba(30, 30, 34, 255));
    }
    ImageResizeNN(&img, size, size);
    return img;
}

Image renderCoreSprite(int size) {
    const int big = size * kSupersample;
    Image img = GenImageColor(big, big, Color{0, 0, 0, 0});
    const int c = big / 2;
    const int r = big * 0.34;
    for (int y = -r; y <= r; ++y) {
        const int half = static_cast<int>(r * std::sqrt(std::max(0.0f, 1.0f -
                                          (y * y) / (float)(r * r))));
        ImageDrawRectangle(&img, c - half, c + y, half * 2, 1,
                           y < 0 ? rgba(140, 226, 255, 255) : rgba(60, 140, 210, 255));
    }
    for (int i = 0; i < 3; ++i) {
        ImageDrawCircle(&img, c, c, r + 4 - i, Color{120, 210, 255, 30});
    }
    ImageResizeNN(&img, size, size);
    return img;
}

Image renderTowerIcon(TowerKind kind, int size) {
    return renderTowerSprite(kind, 1, size);
}

}  // namespace tf
