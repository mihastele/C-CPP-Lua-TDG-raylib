// towerfall — sprite sheet previewer.
//
// Generates every sprite the game uses and writes them into one PNG, so the
// artwork can be inspected without launching the game. Run it after changing
// any drawing code:
//
//     towerfall_sprites out.png
//
// It also prints a summary, so a missing tower or enemy shows up as a count
// mismatch rather than as an empty space in the sheet.

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "raylib.h"

#include "core/Defs.h"
#include "render/Sprites.h"

using namespace tf;

// Blits `src` onto `dst` at (ox, oy), honouring src's alpha.
// raylib has no ImageDrawTexture, so this is done by hand.
void blit(Image& dst, const Image& src, int ox, int oy) {
    for (int y = 0; y < src.height; ++y) {
        for (int x = 0; x < src.width; ++x) {
            const Color s = static_cast<const Color*>(src.data)[y * src.width + x];
            if (s.a == 0) continue;
            const int dx = ox + x, dy = oy + y;
            if (dx < 0 || dy < 0 || dx >= dst.width || dy >= dst.height) continue;
            Color d = static_cast<Color*>(dst.data)[dy * dst.width + dx];
            const float a = s.a / 255.0f;
            d.r = static_cast<unsigned char>(s.r * a + d.r * (1 - a));
            d.g = static_cast<unsigned char>(s.g * a + d.g * (1 - a));
            d.b = static_cast<unsigned char>(s.b * a + d.b * (1 - a));
            d.a = 255;
            static_cast<Color*>(dst.data)[dy * dst.width + dx] = d;
        }
    }
}

int main(int argc, char** argv) {
    const std::string out = (argc > 1) ? argv[1] : "sprites.png";
    const int cell = 96;
    const int pad = 8;

    // ---- gather the sprites
    struct Entry {
        Image image;
        std::string label;
    };
    std::vector<Entry> entries;

    for (const auto& def : allTowerDefs()) {
        for (int level = 1; level <= def.maxLevel(); ++level) {
            entries.push_back({renderTowerSprite(def.kind, level, cell),
                               def.name + " Lv" + std::to_string(level)});
        }
    }
    for (const auto& def : allEnemyDefs()) {
        entries.push_back({renderEnemySprite(def.kind, def.shape, def.color, def.scale, cell),
                           def.name});
    }
    entries.push_back({renderHeartSprite(cell), "heart"});
    entries.push_back({renderCoinSprite(cell), "coin"});
    entries.push_back({renderSkullSprite(cell), "skull"});
    entries.push_back({renderCoreSprite(cell), "core"});

    // ---- lay them out in a grid
    const int cols = 8;
    const int rows = (static_cast<int>(entries.size()) + cols - 1) / cols;
    const int sheetW = cols * (cell + pad) + pad;
    const int sheetH = rows * (cell + pad + 18) + pad;

    Image sheet = GenImageColor(sheetW, sheetH, Color{28, 30, 36, 255});

    // A subtle checker so transparency is visible.
    for (int y = 0; y < sheetH; y += 16) {
        for (int x = 0; x < sheetW; x += 16) {
            if (((x / 16) + (y / 16)) % 2 == 0) {
                ImageDrawRectangle(&sheet, x, y, 16, 16, Color{34, 36, 44, 255});
            }
        }
    }

    for (size_t i = 0; i < entries.size(); ++i) {
        const int cx = pad + static_cast<int>(i) % cols * (cell + pad);
        const int cy = pad + static_cast<int>(i) / cols * (cell + pad + 18);
        blit(sheet, entries[i].image, cx, cy);
        // An outline, drawn as four thin bars. ImageDrawRectangle draws a
        // FILLED rectangle, so using it here would cover the sprite entirely.
        const Color border{70, 74, 86, 255};
        ImageDrawRectangle(&sheet, cx - 1, cy - 1, cell + 2, 1, border);
        ImageDrawRectangle(&sheet, cx - 1, cy + cell, cell + 2, 1, border);
        ImageDrawRectangle(&sheet, cx - 1, cy - 1, 1, cell + 2, border);
        ImageDrawRectangle(&sheet, cx + cell, cy - 1, 1, cell + 2, border);
        // The label, as a crude bitmap so no font is needed.
        for (size_t ch = 0; ch < entries[i].label.size() && ch < 18; ++ch) {
            ImageDrawRectangle(&sheet, cx + static_cast<int>(ch) * 5, cy + cell + 4,
                               4, 6, Color{190, 196, 210, 255});
        }
        std::printf("%2zu  %s\n", i, entries[i].label.c_str());
    }

    ExportImage(sheet, out.c_str());
    std::printf("\n%d sprites -> %s (%dx%d)\n", static_cast<int>(entries.size()),
                out.c_str(), sheetW, sheetH);

    // ---- a quick sanity check: a sprite that is entirely transparent is a bug.
    int blank = 0;
    for (const auto& e : entries) {
        bool any = false;
        for (int y = 0; y < e.image.height && !any; y += 2) {
            for (int x = 0; x < e.image.width; x += 2) {
                if (static_cast<const Color*>(e.image.data)[y * e.image.width + x].a != 0) {
                    any = true;
                    break;
                }
            }
        }
        if (!any) {
            std::printf("!! blank sprite: %s\n", e.label.c_str());
            ++blank;
        }
    }
    if (blank > 0) {
        std::printf("%d sprite(s) are blank\n", blank);
        return 1;
    }
    std::printf("all sprites have pixels\n");
    return 0;
}
