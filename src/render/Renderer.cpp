// towerfall — the renderer.
//
// Draws the map, the towers, the enemies, the projectiles and the whole user
// interface. It reads the simulation and never writes to it; every state
// change goes back through the Game class.

#include "render/Renderer.h"

#include <cmath>
#include <string>

#include "render/Sprites.h"

namespace tf {
namespace {

// ---- palette ------------------------------------------------------------

const Color kBg{22, 24, 30, 255};
const Color kPanel{32, 35, 44, 255};
const Color kPanelLight{42, 46, 58, 255};
const Color kGrid{44, 48, 60, 255};
const Color kText{228, 232, 240, 255};
const Color kTextDim{146, 154, 170, 255};
const Color kGold{246, 200, 82, 255};
const Color kRed{226, 82, 78, 255};
const Color kGreen{96, 200, 116, 255};

/**
 * Draws a texture scaled into a square of `size` pixels at (x, y).
 *
 * raylib 5.5's DrawTexture() has no width/height parameters — the size comes from the
 * texture itself — so DrawTexturePro is used to scale it.
 */
void drawScaled(Texture2D tex, int x, int y, int size, Color tint) {
    DrawTexturePro(tex,
                   Rectangle{0.0f, 0.0f, static_cast<float>(tex.width),
                             static_cast<float>(tex.height)},
                   Rectangle{static_cast<float>(x), static_cast<float>(y),
                             static_cast<float>(size), static_cast<float>(size)},
                   Vector2{0, 0}, 0.0f, tint);
}

/** Draws a texture centered at (cx, cy) rotated by rotDeg degrees. */
void drawRotated(Texture2D tex, float cx, float cy, float size, float rotDeg, Color tint) {
    DrawTexturePro(tex,
                   Rectangle{0.0f, 0.0f, static_cast<float>(tex.width),
                             static_cast<float>(tex.height)},
                   Rectangle{cx, cy, size, size},
                   Vector2{size * 0.5f, size * 0.5f},
                   rotDeg, tint);
}

/** The distance between two points. raylib 5.5 has no Vector2Distance. */
float dist2(Vector2 a, Vector2 b) {
    const float dx = a.x - b.x, dy = a.y - b.y;
    return std::sqrt(dx * dx + dy * dy);
}

void drawText(const std::string& s, int x, int y, int size, Color c) {
    DrawText(s.c_str(), x, y, size, c);
}

void drawTextCentered(const std::string& s, int cx, int y, int size, Color c) {
    const int w = MeasureText(s.c_str(), size);
    DrawText(s.c_str(), cx - w / 2, y, size, c);
}

// ---- terrain ------------------------------------------------------------

void drawTerrainTile(int px, int py, int ts, Terrain t, int seed) {
    switch (t) {
        case Terrain::Grass: {
            DrawRectangle(px, py, ts, ts, Color{58, 82, 54, 255});
            // Tufts of grass, placed deterministically so they never flicker.
            const int n = 2 + (seed % 3);
            for (int i = 0; i < n; ++i) {
                const int h = (seed * 7 + i * 13) % 5 + 3;
                const int ox = (seed * 31 + i * 17) % ts;
                const int oy = (seed * 11 + i * 23) % ts;
                DrawRectangle(px + ox, py + oy - h, 2, h, Color{72, 100, 64, 255});
            }
            break;
        }
        case Terrain::Path: {
            DrawRectangle(px, py, ts, ts, Color{126, 112, 84, 255});
            // A worn track down the middle of the tile.
            DrawRectangle(px, py + static_cast<int>(ts * 0.3f), ts,
                          static_cast<int>(ts * 0.4f), Color{140, 126, 96, 255});
            for (int i = 0; i < 3; ++i) {
                const int ox = (seed * 19 + i * 29) % ts;
                const int oy = (seed * 23 + i * 11) % ts;
                DrawRectangle(px + ox, py + oy, 3, 2, Color{112, 100, 76, 255});
            }
            break;
        }
        case Terrain::Rock: {
            DrawRectangle(px, py, ts, ts, Color{64, 64, 70, 255});
            const int r = static_cast<int>(ts * 0.34f);
            DrawRectangle(px + ts / 2 - r, py + ts / 2 - static_cast<int>(r * 0.7f),
                          r * 2, static_cast<int>(r * 1.4f), Color{94, 94, 102, 255});
            DrawRectangle(px + ts / 2 - static_cast<int>(r * 0.6f),
                          py + ts / 2 - static_cast<int>(r * 0.9f),
                          static_cast<int>(r * 1.2f), static_cast<int>(r * 0.8f),
                          Color{116, 116, 126, 255});
            break;
        }
        case Terrain::Water: {
            DrawRectangle(px, py, ts, ts, Color{44, 82, 122, 255});
            for (int i = 0; i < 3; ++i) {
                const int oy = (seed * 13 + i * 17) % ts;
                DrawRectangle(px + (seed * 7 + i * 23) % ts, py + oy, ts / 2, 2,
                              Color{70, 118, 164, 255});
            }
            break;
        }
        case Terrain::Sand:
            DrawRectangle(px, py, ts, ts, Color{142, 126, 88, 255});
            break;
        default:
            DrawRectangle(px, py, ts, ts, kBg);
            break;
    }
}

}  // namespace

// ------------------------------------------------------------------ sprites

SpriteSet buildSprites() {
    SpriteSet s;

    // One texture per (tower, level).
    for (const auto& def : allTowerDefs()) {
        for (int level = 1; level <= def.maxLevel(); ++level) {
            Image img = renderTowerSprite(def.kind, level, 64);
            s.towerSprites.push_back(LoadTextureFromImage(img));
            UnloadImage(img);
        }
    }
    for (const auto& def : allEnemyDefs()) {
        Image img = renderEnemySprite(def.kind, def.shape, def.color, def.scale, 64);
        s.enemySprites.push_back(LoadTextureFromImage(img));
        UnloadImage(img);
    }

    Image h = renderHeartSprite(32);
    s.heart = LoadTextureFromImage(h); UnloadImage(h);
    Image c = renderCoinSprite(32);
    s.coin = LoadTextureFromImage(c); UnloadImage(c);
    Image k = renderSkullSprite(32);
    s.skull = LoadTextureFromImage(k); UnloadImage(k);
    Image core = renderCoreSprite(48);
    s.coreIcon = LoadTextureFromImage(core); UnloadImage(core);

    return s;
}

void freeSprites(SpriteSet& s) {
    for (auto& t : s.towerSprites) UnloadTexture(t);
    for (auto& t : s.enemySprites) UnloadTexture(t);
    s.towerSprites.clear();
    s.enemySprites.clear();
}

Color paletteColor(const std::string& name) {
    if (name == "red") return kRed;
    if (name == "gold") return kGold;
    if (name == "green") return kGreen;
    return Color{110, 180, 240, 255};
}

// ------------------------------------------------------------------ indices

int towerTextureIndex(TowerKind kind, int level) {
    int index = 0;
    for (const auto& def : allTowerDefs()) {
        if (def.kind == kind) return index + (level - 1);
        index += def.maxLevel();
    }
    return 0;
}

int enemyTextureIndex(EnemyKind kind) {
    int index = 0;
    for (const auto& def : allEnemyDefs()) {
        if (def.kind == kind) return index;
        ++index;
    }
    return 0;
}

// ------------------------------------------------------------------ renderer

void Renderer::init() {
    sprites_ = buildSprites();
}

void Renderer::shutdown() {
    freeSprites(sprites_);
}

void Renderer::draw(const Game& g, const GameView& view) {
    const int screenW = GetScreenWidth();
    const int screenH = GetScreenHeight();
    const int sideW = 300;
    const int canvasW = screenW - sideW;

    DrawRectangle(0, 0, screenW, screenH, kBg);

    // ---- the tile size, so the whole level fits the canvas
    const int ts = std::min(static_cast<int>(canvasW / g.grid().width()),
                            static_cast<int>((screenH - 40) / g.grid().height()));
    const int mapW = ts * g.grid().width();
    const int mapH = ts * g.grid().height();
    const int mapX = (canvasW - mapW) / 2;
    const int mapY = 40 + (screenH - 40 - mapH) / 2;

    // THE COORDINATE-SPACE BRIDGE — see MapView in Renderer.h. The renderer and the
    // input code share this one object, so a click and a drawn enemy can never
    // disagree about where a cell is.
    const MapView mapView{static_cast<float>(mapX), static_cast<float>(mapY),
                          static_cast<float>(ts), g.tileSize()};

    // The core Vec2 -> raylib Vector2, at the last possible moment.
    auto cellPx = [&](Cell c) {
        const Vec2 s = mapView.cellToScreen(c);
        return Vector2{s.x, s.y};
    };
    auto worldToScreen = [&](Vec2 w) {
        const Vec2 s = mapView.worldToScreen(w);
        return Vector2{s.x, s.y};
    };

    // ---- terrain
    for (int y = 0; y < g.grid().height(); ++y) {
        for (int x = 0; x < g.grid().width(); ++x) {
            const int seed = (x * 73856093) ^ (y * 19349663);
            drawTerrainTile(mapX + x * ts, mapY + y * ts, ts, g.grid().at(x, y), seed);
        }
    }

    // ---- the intended route, drawn as a dotted trail so the player can see
    //      where the enemies are heading. It is a hint, not a rule.
    const auto& route = g.grid().pathTiles();
    for (size_t i = 0; i + 1 < route.size(); ++i) {
        const Vector2 a = cellPx(route[i]);
        const Vector2 b = cellPx(route[i + 1]);
        const float steps = static_cast<float>(ts) * 0.5f;
        const int n = std::max(1, static_cast<int>(dist2(a, b) / steps));
        for (int k = 0; k < n; ++k) {
            const float t = static_cast<float>(k) / n;
            const float px = a.x + (b.x - a.x) * t + ts * 0.5f;
            const float py = a.y + (b.y - a.y) * t + ts * 0.5f;
            DrawCircleV({px, py}, 2.0f, Color{190, 176, 140, 110});
        }
    }

    // ---- the entry: a dark hole the enemies come out of
    {
        Cell s{-1, -1};
        for (int y = 0; y < g.grid().height() && s.x < 0; ++y) {
            for (int x = 0; x < g.grid().width(); ++x) {
                if (x == 0 && g.grid().at(x, y) == Terrain::Path) { s = Cell{x, y}; break; }
            }
        }
        if (s.x >= 0) {
            const Vector2 p = cellPx(s);
            DrawCircleV({p.x + ts * 0.5f, p.y + ts * 0.5f}, ts * 0.34f, Color{26, 22, 30, 255});
            DrawCircleLinesV({p.x + ts * 0.5f, p.y + ts * 0.5f}, ts * 0.34f, Color{130, 100, 150, 200});
        }
    }

    // ---- the core at the exit
    {
        Cell e{-1, -1};
        for (int y = 0; y < g.grid().height() && e.x < 0; ++y) {
            for (int x = g.grid().width() - 1; x >= 0; --x) {
                if (g.grid().at(x, y) == Terrain::Path) { e = Cell{x, y}; break; }
            }
        }
        if (e.x >= 0) {
            const Vector2 p = cellPx(e);
            drawScaled(sprites_.coreIcon, static_cast<int>(p.x) - 24,
                       static_cast<int>(p.y) - 24, 48, Color{255, 255, 255, 255});
        }
    }

    // ---- range previews
    if (view.mode == ToolMode::Build && view.hover.x >= 0) {
        const TowerDef& def = towerDef(view.buildKind);
        const float r = def.level(0).range * ts;
        const Vector2 p = cellPx(view.hover);
        DrawCircleV({p.x + ts * 0.5f, p.y + ts * 0.5f}, r, Color{120, 200, 255, 40});
        DrawCircleLinesV({p.x + ts * 0.5f, p.y + ts * 0.5f}, r, Color{120, 200, 255, 120});
    }
    if (view.selectedTowerId != 0 && view.showRanges) {
        for (const auto& t : g.towers()) {
            if (t.id != view.selectedTowerId) continue;
            const TowerDef& def = towerDef(t.kind);
            const float r = def.level(t.level - 1).range * ts;
            const Vector2 p = cellPx(t.cell);
            DrawCircleV({p.x + ts * 0.5f, p.y + ts * 0.5f}, r, Color{255, 220, 120, 26});
            DrawCircleLinesV({p.x + ts * 0.5f, p.y + ts * 0.5f}, r, Color{255, 220, 120, 90});
        }
    }

    // ---- the build ghost
    if (view.mode == ToolMode::Build && view.hover.x >= 0) {
        const Vector2 p = cellPx(view.hover);
        DrawRectangle(p.x, p.y, ts, ts, Color{120, 200, 255, 50});
    }

    // ---- towers
    for (const auto& t : g.towers()) {
        const Vector2 p = cellPx(t.cell);
        const int texIndex = towerTextureIndex(t.kind, t.level);
        if (texIndex >= 0 && texIndex < static_cast<int>(sprites_.towerSprites.size())) {
            const float cx = p.x + ts * 0.5f;
            const float cy = p.y + ts * 0.5f;
            const float rotDeg = t.angle * (180.0f / 3.1415926535f);
            drawRotated(sprites_.towerSprites[static_cast<size_t>(texIndex)],
                        cx, cy, static_cast<float>(ts), rotDeg, Color{255, 255, 255, 255});
        }
        for (int i = 0; i < t.level; ++i) {
            DrawRectangle(p.x + 3 + i * 6, p.y + ts - 7, 4, 4, kGold);
        }
        if (t.hp < t.maxHp) {
            const float w = ts - 6;
            DrawRectangle(p.x + 3, p.y + 2, w, 4, Color{30, 30, 34, 220});
            DrawRectangle(p.x + 3, p.y + 2, w * (t.hp / static_cast<float>(t.maxHp)), 4,
                          Color{120, 200, 120, 240});
        }
        if (t.kind == TowerKind::Beacon) {
            const TowerDef& def = towerDef(t.kind);
            DrawCircleLinesV({p.x + ts * 0.5f, p.y + ts * 0.5f},
                            def.level(t.level - 1).range * ts, Color{255, 220, 120, 60});
        }
    }

    // ---- enemies
    for (const auto& e : g.enemies()) {
        const EnemyDef& def = enemyDef(e.kind);
        const int texIndex = enemyTextureIndex(e.kind);
        float size = ts * def.scale * 0.9f;
        if (e.isBoss) size = ts * def.scale * 1.05f;
        const float alpha = 255.0f * (0.35f + 0.65f * (1.0f - e.spawnFade));
        Color tint{255, 255, 255, static_cast<unsigned char>(alpha)};
        if (e.hitFlash > 0) tint = Color{255, 200, 200, static_cast<unsigned char>(alpha)};
        if (e.slowFactor < 1.0f) tint.r = static_cast<unsigned char>(tint.r * 0.7f);
        const Vector2 ep = worldToScreen(Vec2{e.x, e.y});
        const float rotDeg = e.angle * (180.0f / 3.1415926535f);
        drawRotated(sprites_.enemySprites[static_cast<size_t>(texIndex)],
                    ep.x, ep.y, size, rotDeg, tint);

        if (e.maxHp > 60 || e.isBoss) {
            const float w = size * 0.9f;
            DrawRectangle(ep.x - w * 0.5f, ep.y - size * 0.6f, w, 4, Color{26, 26, 30, 220});
            const float frac = std::max(0.0f, e.hp / static_cast<float>(e.maxHp));
            DrawRectangle(ep.x - w * 0.5f, ep.y - size * 0.6f, w * frac, 4,
                          e.isBoss ? Color{220, 90, 90, 255} : Color{240, 200, 90, 255});
        }
        float sy = ep.y - size * 0.75f;
        if (e.slowTimer > 0)   { DrawCircleV({ep.x - 8, sy}, 4, Color{110, 190, 240, 255}); sy -= 9; }
        if (e.burnTimer > 0)   { DrawCircleV({ep.x - 8, sy}, 4, Color{240, 140, 60, 255}); sy -= 9; }
        if (e.poisonTimer > 0) { DrawCircleV({ep.x - 8, sy}, 4, Color{140, 220, 90, 255}); sy -= 9; }
        if (e.archetype == EnemyArchetype::Attacker) {
            DrawCircleLinesV(ep, size * 0.62f, Color{240, 90, 90, 160});
        }
    }

    // ---- projectiles
    for (const auto& p : g.projectiles()) {
        Color c{255, 240, 200, 255};
        float r = 3;
        switch (p.style) {
            case ProjectileStyle::Cannonball:  c = Color{70, 70, 78, 255}; r = 5; break;
            case ProjectileStyle::FrostShard:  c = Color{160, 220, 250, 255}; r = 4; break;
            case ProjectileStyle::Lightning:   c = Color{130, 220, 255, 255}; r = 3; break;
            case ProjectileStyle::Bullet:      c = Color{250, 250, 220, 255}; r = 2; break;
            case ProjectileStyle::PoisonBottle:c = Color{170, 230, 90, 255}; r = 4; break;
            case ProjectileStyle::Fireball:    c = Color{255, 170, 70, 255}; r = 5; break;
            case ProjectileStyle::Prism:       c = Color{230, 200, 255, 255}; r = 3; break;
            case ProjectileStyle::MortarShell: c = Color{90, 84, 76, 255}; r = 6; break;
            case ProjectileStyle::Pierce:      c = Color{240, 246, 255, 255}; r = 3; break;
            default: break;
        }
        DrawCircleV(worldToScreen(Vec2{p.pos.x, p.pos.y}), r, c);
    }

    // ---- floating text
    for (const auto& f : g.floaters()) {
        const float a = std::max(0.0f, f.life / f.maxLife);
        const Vector2 fp = worldToScreen(f.pos);
        drawTextCentered(f.text, static_cast<int>(fp.x),
                         static_cast<int>(fp.y - (1.0f - a) * 18), 16,
                         Color{static_cast<unsigned char>(f.color.r * a + 60 * (1 - a)),
                               static_cast<unsigned char>(f.color.g * a + 60 * (1 - a)),
                               static_cast<unsigned char>(f.color.b * a + 60 * (1 - a)),
                               255});
    }

    // ---- the top bar
    drawText(g.level().name, 14, 10, 20, kText);
    drawText(g.level().difficulty, 16 + MeasureText(g.level().name.c_str(), 20), 14, 13, kTextDim);

    drawScaled(sprites_.heart, canvasW - 250, 8, 24, Color{255, 255, 255, 255});
    drawText(std::to_string(g.lives()), canvasW - 222, 13, 18, kText);
    drawScaled(sprites_.coin, canvasW - 170, 8, 24, Color{255, 255, 255, 255});
    drawText(std::to_string(g.gold()), canvasW - 142, 13, 18, kGold);
    drawText("WAVE " + std::to_string(g.waveNumber()), canvasW - 96, 13, 18, kText);

    if (g.isWaveActive()) {
        drawTextCentered(waveDef(g.waveNumber()).name, canvasW / 2, 10, 16, kTextDim);
    } else {
        const int secs = static_cast<int>(std::ceil(g.prepTimer()));
        drawTextCentered("Next wave in " + std::to_string(secs) + "s   (SPACE to call it early)",
                         canvasW / 2, 10, 16, kGold);
    }

    // ---- the sidebar
    const int sx = canvasW;
    DrawRectangle(sx, 0, sideW, screenH, kPanel);
    DrawRectangle(sx, 0, 1, screenH, kGrid);

    int y = 12;
    drawText("TOWERS", sx + 14, y, 15, kTextDim);
    y += 24;

    for (const auto& def : allTowerDefs()) {
        const bool affordable = g.gold() >= def.level(0).cost;
        const bool selected = view.mode == ToolMode::Build && view.buildKind == def.kind;
        DrawRectangle(sx + 10, y, sideW - 20, 42, selected ? kPanelLight : kPanel);
        if (selected) DrawRectangle(sx + 10, y, 3, 42, kGold);

        const int texIndex = towerTextureIndex(def.kind, 1);
        if (texIndex >= 0 && texIndex < static_cast<int>(sprites_.towerSprites.size())) {
            drawScaled(sprites_.towerSprites[static_cast<size_t>(texIndex)],
                       sx + 16, y + 4, 34,
                       affordable ? Color{255, 255, 255, 255} : Color{120, 120, 128, 255});
        }
        drawText(def.name, sx + 56, y + 5, 15, affordable ? kText : kTextDim);
        drawText(def.description.size() > 32 ? def.description.substr(0, 31) + "…" : def.description,
                 sx + 56, y + 24, 11, kTextDim);
        drawText(std::to_string(def.level(0).cost) + "g", sx + sideW - 60, y + 5, 14,
                 affordable ? kGold : Color{150, 120, 70, 255});
        y += 46;
    }

    // ---- the selected tower's panel
    const Tower* sel = nullptr;
    for (const auto& t : g.towers()) if (t.id == view.selectedTowerId) sel = &t;
    if (sel) {
        y += 8;
        DrawRectangle(sx + 10, y, sideW - 20, 2, kGrid);
        y += 12;
        const TowerDef& def = towerDef(sel->kind);
        drawText(def.name + "  Lv" + std::to_string(sel->level), sx + 14, y, 16, kText);
        y += 24;
        drawText("Damage  " + std::to_string(def.level(sel->level - 1).damage), sx + 14, y, 13, kTextDim);
        y += 17;
        drawText("Range    " + std::to_string(static_cast<int>(def.level(sel->level - 1).range)) + " tiles",
                 sx + 14, y, 13, kTextDim);
        y += 17;
        drawText("Rate     " + std::to_string(def.level(sel->level - 1).fireRate) + "/s",
                 sx + 14, y, 13, kTextDim);
        y += 24;

        if (sel->level < def.maxLevel()) {
            const int cost = def.level(sel->level).cost;
            const bool can = g.gold() >= cost;
            DrawRectangle(sx + 14, y, sideW - 28, 30, can ? kPanelLight : kPanel);
            drawText("UPGRADE  (" + std::to_string(cost) + "g)  [U]", sx + 22, y + 8, 13,
                     can ? kGold : kTextDim);
        } else {
            drawText("MAX LEVEL", sx + 14, y + 8, 14, kTextDim);
        }
        y += 38;
        DrawRectangle(sx + 14, y, sideW - 28, 30, kPanelLight);
        drawText("SELL  [X]", sx + 22, y + 8, 14, kText);
    }

    // ---- the status message
    if (!g.statusMessage().empty()) {
        const std::string& m = g.statusMessage();
        const int w = MeasureText(m.c_str(), 16);
        DrawRectangle(canvasW / 2 - w / 2 - 12, screenH - 44, w + 24, 28, Color{20, 22, 28, 200});
        drawTextCentered(m, canvasW / 2, screenH - 38, 16, kText);
    }

    // ---- the end screens
    if (g.isGameOver()) {
        DrawRectangle(0, 0, screenW, screenH, Color{10, 8, 12, 210});
        drawTextCentered("THE CORE WAS DESTROYED", screenW / 2, screenH / 2 - 80, 40, kRed);
        drawTextCentered("Wave " + std::to_string(g.waveNumber()) + "   ·   " +
                         std::to_string(g.kills()) + " kills   ·   " +
                         std::to_string(g.score()) + " points",
                         screenW / 2, screenH / 2 - 30, 20, kText);
        drawTextCentered("Press R to try again", screenW / 2, screenH / 2 + 20, 20, kTextDim);
    } else if (g.isVictory()) {
        DrawRectangle(0, 0, screenW, screenH, Color{10, 12, 16, 210});
        drawTextCentered("ALL WAVES CLEARED", screenW / 2, screenH / 2 - 80, 40, kGold);
        drawTextCentered(std::to_string(g.kills()) + " kills   ·   " +
                         std::to_string(g.score()) + " points   ·   " +
                         std::to_string(g.lives()) + " lives left",
                         screenW / 2, screenH / 2 - 30, 20, kText);
        drawTextCentered("Press R to play again", screenW / 2, screenH / 2 + 20, 20, kTextDim);
    }
}

}  // namespace tf
