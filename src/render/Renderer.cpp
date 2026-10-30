// towerfall — the renderer.
//
// Draws the map, the towers, the enemies, the projectiles and the whole user
// interface. It reads the simulation and never writes to it; every state
// change goes back through the Game class.

#include "render/Renderer.h"

#include <algorithm>
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
    particles_.clear();
    shockwaves_.clear();
    screenShake_ = 0.0f;
    lastProcessedEventIndex_ = 0;
    audio_.init();
}

void Renderer::shutdown() {
    audio_.shutdown();
    freeSprites(sprites_);
    particles_.clear();
    shockwaves_.clear();
}

void Renderer::addExplosion(Vector2 pos, Color c, int count, float speed) {
    for (int i = 0; i < count; ++i) {
        const float a = static_cast<float>(rand() % 360) * 0.0174533f;
        const float spd = (speed * 0.4f) + static_cast<float>(rand() % 100) * 0.01f * (speed * 0.6f);
        Particle p;
        p.pos = pos;
        p.vel = Vector2{std::cos(a) * spd, std::sin(a) * spd};
        p.color = c;
        p.life = 0.4f + static_cast<float>(rand() % 100) * 0.003f;
        p.maxLife = p.life;
        p.size = 2.0f + static_cast<float>(rand() % 3);
        particles_.push_back(p);
    }
}

void Renderer::addShockwave(Vector2 pos, float maxR, Color c, float life) {
    Shockwave s;
    s.pos = pos;
    s.radius = 2.0f;
    s.maxRadius = maxR;
    s.color = c;
    s.life = life;
    s.maxLife = life;
    shockwaves_.push_back(s);
}

void Renderer::triggerScreenShake(float intensity) {
    screenShake_ = std::max(screenShake_, intensity);
}

void Renderer::processEvents(const Game& g, const MapView& mapView) {
    const auto& evs = g.events();
    if (evs.size() < lastProcessedEventIndex_) {
        lastProcessedEventIndex_ = 0;
    }
    while (lastProcessedEventIndex_ < evs.size()) {
        const auto& ev = evs[lastProcessedEventIndex_++];
        const Vec2 s = mapView.worldToScreen(ev.pos);
        const Vector2 sp{s.x, s.y};
        switch (ev.type) {
            case GameEvent::Built:
                audio_.play(SoundId::Build);
                break;
            case GameEvent::Upgraded:
                audio_.play(SoundId::Upgrade);
                break;
            case GameEvent::Sold:
                audio_.play(SoundId::Sell);
                break;
            case GameEvent::TowerRepaired:
                audio_.play(SoundId::Repair);
                break;
            case GameEvent::TowerOverdrive:
                audio_.play(SoundId::Overdrive);
                break;
            case GameEvent::TowerDestroyed:
                audio_.play(SoundId::Explosion);
                addExplosion(sp, Color{255, 140, 50, 255}, 18, 85.0f);
                addShockwave(sp, 45.0f, Color{255, 180, 70, 220}, 0.35f);
                triggerScreenShake(0.35f);
                break;
            case GameEvent::TitanAbility:
                audio_.play(SoundId::TitanRoar);
                if (ev.value == 1) { // Stomp
                    addShockwave(sp, 85.0f, Color{255, 210, 80, 240}, 0.55f);
                    addExplosion(sp, Color{235, 190, 100, 255}, 24, 95.0f);
                    triggerScreenShake(0.55f);
                } else { // Cleave
                    addShockwave(sp, 65.0f, Color{255, 60, 60, 240}, 0.45f);
                    addExplosion(sp, Color{255, 75, 75, 255}, 18, 85.0f);
                    triggerScreenShake(0.40f);
                }
                break;
            case GameEvent::TitanSurge:
                audio_.play(SoundId::SurgeHorn);
                triggerScreenShake(0.6f);
                break;
            case GameEvent::EnemyLeaked:
                audio_.play(SoundId::Hit);
                triggerScreenShake(0.30f);
                break;
            case GameEvent::EnemyKilled:
                audio_.play(SoundId::Hit, 1.0f, 0.35f);
                addExplosion(sp, Color{255, 220, 110, 255}, 6, 40.0f);
                break;
            case GameEvent::MeteorImpact:
                audio_.play(SoundId::Meteor);
                addExplosion(sp, Color{255, 120, 30, 255}, 38, 130.0f);
                addExplosion(sp, Color{255, 230, 80, 255}, 24, 85.0f);
                addShockwave(sp, 95.0f, Color{255, 140, 40, 240}, 0.65f);
                triggerScreenShake(0.65f);
                break;
            case GameEvent::CryoFreezeCast:
                audio_.play(SoundId::CryoFreeze);
                addExplosion(sp, Color{100, 220, 255, 255}, 30, 95.0f);
                addShockwave(sp, 110.0f, Color{120, 230, 255, 200}, 0.50f);
                triggerScreenShake(0.25f);
                break;
            case GameEvent::MidasRushCast:
                audio_.play(SoundId::PowerupCollect, 1.0f, 1.0f);
                addExplosion(sp, Color{255, 220, 60, 255}, 32, 100.0f);
                addShockwave(sp, 70.0f, Color{255, 215, 80, 220}, 0.45f);
                triggerScreenShake(0.20f);
                break;
            case GameEvent::PowerupCollected:
                audio_.play(SoundId::PowerupCollect);
                addExplosion(sp, Color{255, 230, 100, 255}, 14, 60.0f);
                break;
            case GameEvent::ComboTriggered:
                audio_.play(SoundId::ComboTrigger, 1.0f, 0.75f);
                addExplosion(sp, Color{255, 160, 40, 255}, 14, 75.0f);
                addShockwave(sp, 45.0f, Color{255, 200, 80, 220}, 0.30f);
                break;
            case GameEvent::Victory:
                audio_.play(SoundId::Victory);
                break;
            case GameEvent::GameOver:
                audio_.play(SoundId::GameOver);
                break;
            default: break;
        }
    }
}

void Renderer::draw(const Game& g, const GameView& view) {
    const int screenW = GetScreenWidth();
    const int screenH = GetScreenHeight();
    const int sideW = 300;
    const int canvasW = screenW - sideW;

    DrawRectangle(0, 0, screenW, screenH, kBg);

    const float dt = GetFrameTime();
    if (screenShake_ > 0.0f) {
        screenShake_ = std::max(0.0f, screenShake_ - dt * 2.5f);
    }
    const float shakeOffset = (screenShake_ > 0.0f) ? (std::sin(view.time * 50.0f) * screenShake_ * 8.0f) : 0.0f;

    // ---- the tile size, so the whole level fits the canvas
    const int ts = std::min(static_cast<int>(canvasW / g.grid().width()),
                            static_cast<int>((screenH - 40) / g.grid().height()));
    const int mapW = ts * g.grid().width();
    const int mapH = ts * g.grid().height();
    const int mapX = (canvasW - mapW) / 2 + static_cast<int>(shakeOffset);
    const int mapY = 40 + (screenH - 40 - mapH) / 2 + static_cast<int>(shakeOffset * 0.5f);

    // THE COORDINATE-SPACE BRIDGE — see MapView in Renderer.h. The renderer and the
    // input code share this one object, so a click and a drawn enemy can never
    // disagree about where a cell is.
    const MapView mapView{static_cast<float>(mapX), static_cast<float>(mapY),
                          static_cast<float>(ts), g.tileSize()};

    processEvents(g, mapView);

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

    // ---- Active Path & Threat Heatmap Overlay
    if (view.showPathOverlay) {
        // 1. Heatmap: tower coverage density
        for (int y = 0; y < g.grid().height(); ++y) {
            for (int x = 0; x < g.grid().width(); ++x) {
                const Cell c{x, y};
                const Vec2 wp = g.cellToWorld(c);
                int coverage = 0;
                for (const auto& t : g.towers()) {
                    if (g.cellToWorld(t.cell).distanceTo(wp) <= g.effectiveRange(t)) {
                        coverage++;
                    }
                }
                if (coverage > 0) {
                    const Vector2 p = cellPx(c);
                    Color heat = (coverage == 1) ? Color{0, 150, 255, 30} :
                                 (coverage == 2) ? Color{0, 220, 255, 50} :
                                 (coverage <= 4) ? Color{255, 180, 50, 65} :
                                                   Color{255, 60, 40, 80};
                    DrawRectangle(static_cast<int>(p.x), static_cast<int>(p.y), ts, ts, heat);
                }
            }
        }

        // 2. Dynamic ground creep flow route
        std::vector<Cell> currentPath = g.grid().findPath(g.start(), g.goal());
        for (size_t i = 0; i + 1 < currentPath.size(); ++i) {
            const Vector2 a = cellPx(currentPath[i]);
            const Vector2 b = cellPx(currentPath[i + 1]);
            const float cx1 = a.x + ts * 0.5f;
            const float cy1 = a.y + ts * 0.5f;
            const float cx2 = b.x + ts * 0.5f;
            const float cy2 = b.y + ts * 0.5f;
            DrawLineEx({cx1, cy1}, {cx2, cy2}, 3.0f, Color{80, 240, 140, 170});
            const float pulseOffset = std::fmod(view.time * 2.5f + static_cast<float>(i) * 0.25f, 1.0f);
            const float dotX = cx1 + (cx2 - cx1) * pulseOffset;
            const float dotY = cy1 + (cy2 - cy1) * pulseOffset;
            DrawCircleV({dotX, dotY}, 3.5f, Color{210, 255, 230, 240});
        }

        // 3. Titan threat danger radius
        for (const auto& e : g.enemies()) {
            if (e.isBoss) {
                const Vector2 ep = worldToScreen(Vec2{e.x, e.y});
                const float stompR = 2.5f * ts;
                DrawCircleLinesV(ep, stompR, Color{255, 80, 50, 160});
                DrawCircleV(ep, stompR, Color{255, 60, 40, 25});
            }
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
        // Shield barrier
        if (t.shieldHp > 0) {
            DrawCircleLinesV({p.x + ts * 0.5f, p.y + ts * 0.5f}, ts * 0.48f, Color{65, 210, 255, 180});
            const float w = ts - 6;
            DrawRectangle(p.x + 3, p.y + ts - 3, w * (std::min(50, t.shieldHp) / 50.0f), 2, Color{60, 225, 255, 255});
        }
        // Overdrive supercharged aura & electrical sparks
        if (t.overdriveRemaining > 0) {
            const float pulse = std::sin(view.time * 24.0f) * 2.0f;
            DrawCircleLinesV({p.x + ts * 0.5f, p.y + ts * 0.5f}, ts * 0.52f + pulse, Color{255, 205, 45, 220});
            const float spAng = view.time * 14.0f;
            Vector2 sp1{p.x + ts * 0.5f + std::cos(spAng) * ts * 0.44f,
                        p.y + ts * 0.5f + std::sin(spAng) * ts * 0.44f};
            DrawCircleV(sp1, 2.5f, Color{255, 245, 120, 255});
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

        // ---- Elite Affix Aura and Visual Indicators
        if (e.affix != EnemyAffix::None) {
            Color auraCol = Color{255, 255, 255, 180};
            switch (e.affix) {
                case EnemyAffix::Frenzied:    auraCol = Color{255, 110, 40, 200}; break;
                case EnemyAffix::Juggernaut:  auraCol = Color{255, 215, 60, 220}; break;
                case EnemyAffix::Regenerator: auraCol = Color{70, 240, 110, 200}; break;
                case EnemyAffix::Vengeful:    auraCol = Color{240, 45, 65, 220}; break;
                case EnemyAffix::Shielded:    auraCol = Color{65, 200, 255, 220}; break;
                default: break;
            }
            const float pulse = 1.0f + 0.10f * std::sin(view.time * 7.0f);
            DrawCircleLinesV(ep, size * 0.65f * pulse, auraCol);

            // Rotating affix energy nodes
            const float aOffset = view.time * 3.0f;
            for (int k = 0; k < 3; ++k) {
                const float a = aOffset + k * (6.2831853f / 3.0f);
                const float nx = ep.x + std::cos(a) * (size * 0.65f * pulse);
                const float ny = ep.y + std::sin(a) * (size * 0.65f * pulse);
                DrawCircleV({nx, ny}, 2.0f, auraCol);
            }

            // Shield barrier ring if active
            if (e.affix == EnemyAffix::Shielded && e.shieldHp > 0) {
                DrawCircleLinesV(ep, size * 0.8f, Color{100, 225, 255, 190});
            }
        }

        drawRotated(sprites_.enemySprites[static_cast<size_t>(texIndex)],
                    ep.x, ep.y, size, rotDeg, tint);

        if (e.maxHp > 60 || e.isBoss) {
            const float w = size * 0.9f;
            DrawRectangle(ep.x - w * 0.5f, ep.y - size * 0.6f, w, 4, Color{26, 26, 30, 220});
            const float frac = std::max(0.0f, e.hp / static_cast<float>(e.maxHp));
            DrawRectangle(ep.x - w * 0.5f, ep.y - size * 0.6f, w * frac, 4,
                          e.isBoss ? Color{220, 90, 90, 255} : Color{240, 200, 90, 255});
        }

        // Small indicator above elite enemies
        if (e.affix != EnemyAffix::None) {
            const char* aName = affixName(e.affix);
            drawTextCentered(aName, static_cast<int>(ep.x), static_cast<int>(ep.y - size * 0.85f), 10,
                             Color{255, 230, 160, 220});
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

    // ---- Powerup Drops
    for (const auto& p : g.powerups()) {
        const Vector2 sp = worldToScreen(p.pos);
        const float dy = std::sin(view.time * 4.5f + static_cast<float>(p.id)) * 4.0f;
        const Vector2 orbPos{sp.x, sp.y + dy};

        Color orbCol = Color{255, 215, 60, 240};
        const char* iconText = "$";
        switch (p.kind) {
            case PowerupKind::GoldCache:     orbCol = Color{255, 220, 50, 240}; iconText = "$"; break;
            case PowerupKind::OverchargeAll: orbCol = Color{255, 140, 40, 240}; iconText = "O"; break;
            case PowerupKind::CryoFreeze:    orbCol = Color{80, 220, 255, 240}; iconText = "C"; break;
            case PowerupKind::CoreLife:      orbCol = Color{255, 70, 90, 240};  iconText = "+"; break;
            default: break;
        }

        const float pulse = 1.0f + 0.15f * std::sin(view.time * 6.0f);
        DrawCircleV(orbPos, 11.0f * pulse, Color{orbCol.r, orbCol.g, orbCol.b, 65});
        DrawCircleV(orbPos, 8.0f, orbCol);
        DrawCircleLinesV(orbPos, 9.5f * pulse, Color{255, 255, 255, 220});
        drawTextCentered(iconText, static_cast<int>(orbPos.x), static_cast<int>(orbPos.y) - 5, 10, Color{20, 20, 25, 255});
    }

    // ---- update & draw shockwaves
    for (auto& sw : shockwaves_) {
        sw.life -= dt;
        const float t = 1.0f - (sw.life / sw.maxLife);
        sw.radius = sw.maxRadius * t;
        const float alpha = std::max(0.0f, (sw.life / sw.maxLife));
        Color col = sw.color;
        col.a = static_cast<unsigned char>(col.a * alpha);
        DrawCircleLinesV(sw.pos, sw.radius, col);
        if (sw.radius > 5.0f) DrawCircleLinesV(sw.pos, sw.radius - 2.0f, col);
    }
    shockwaves_.erase(
        std::remove_if(shockwaves_.begin(), shockwaves_.end(),
                       [](const Shockwave& s) { return s.life <= 0; }),
        shockwaves_.end());

    // ---- update & draw particles
    for (auto& prt : particles_) {
        prt.life -= dt;
        prt.pos.x += prt.vel.x * dt;
        prt.pos.y += prt.vel.y * dt;
        const float alpha = std::max(0.0f, (prt.life / prt.maxLife));
        Color col = prt.color;
        col.a = static_cast<unsigned char>(col.a * alpha);
        DrawCircleV(prt.pos, prt.size * alpha, col);
    }
    particles_.erase(
        std::remove_if(particles_.begin(), particles_.end(),
                       [](const Particle& pr) { return pr.life <= 0; }),
        particles_.end());

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

    // ---- Meteor Targeting Reticle
    if (view.targetingMeteor) {
        const Vector2 mouse = GetMousePosition();
        const float r = 2.5f * ts;
        DrawCircleLinesV(mouse, r, Color{255, 80, 40, 210});
        DrawCircleV(mouse, r, Color{255, 60, 30, 35});
        DrawLine(static_cast<int>(mouse.x - r - 8), static_cast<int>(mouse.y), static_cast<int>(mouse.x + r + 8), static_cast<int>(mouse.y), Color{255, 150, 60, 200});
        DrawLine(static_cast<int>(mouse.x), static_cast<int>(mouse.y - r - 8), static_cast<int>(mouse.x), static_cast<int>(mouse.y + r + 8), Color{255, 150, 60, 200});
        drawTextCentered("ORBITAL METEOR - LEFT CLICK TO STRIKE  (RIGHT CLICK CANCEL)", static_cast<int>(mouse.x), static_cast<int>(mouse.y - r - 20), 12, Color{255, 215, 80, 255});
    }

    // ---- grand titan boss health bar
    const Enemy* activeTitan = nullptr;
    for (const auto& e : g.enemies()) {
        if (e.isBoss && e.hp > 0) {
            if (!activeTitan || e.hp > activeTitan->hp) activeTitan = &e;
        }
    }
    if (activeTitan) {
        const int barW = 440;
        const int barH = 20;
        const int bx = canvasW / 2 - barW / 2;
        const int by = 35;
        DrawRectangle(bx - 3, by - 3, barW + 6, barH + 6, Color{18, 18, 24, 240});
        DrawRectangleLines(bx - 3, by - 3, barW + 6, barH + 6, Color{255, 175, 65, 255});

        // Health Fill
        const float frac = std::max(0.0f, activeTitan->hp / static_cast<float>(activeTitan->maxHp));
        DrawRectangle(bx, by, static_cast<int>(barW * frac), barH, Color{215, 45, 45, 255});
        DrawRectangle(bx, by, static_cast<int>(barW * frac), 4, Color{255, 120, 120, 255});

        // Shield bar
        if (activeTitan->shieldHp > 0 && activeTitan->maxShieldHp > 0) {
            const float sFrac = std::min(1.0f, activeTitan->shieldHp / activeTitan->maxShieldHp);
            DrawRectangle(bx, by + barH - 4, static_cast<int>(barW * sFrac), 4, Color{65, 220, 255, 255});
        }

        std::string titanLabel = (activeTitan->kind == EnemyKind::Boss ? "TITAN COLOSSUS" : "TITAN RAVAGER");
        if (g.titanSurgeTier() > 0) {
            titanLabel += " [SURGE " + std::to_string(g.titanSurgeTier()) + "]";
        }
        if (activeTitan->affix != EnemyAffix::None) {
            titanLabel += " (" + std::string(affixName(activeTitan->affix)) + ")";
        }
        drawTextCentered(titanLabel, canvasW / 2, by + 3, 12, Color{255, 245, 215, 255});

        std::string hpTxt = std::to_string(activeTitan->hp) + " / " + std::to_string(activeTitan->maxHp);
        drawText(hpTxt, bx + barW - MeasureText(hpTxt.c_str(), 10) - 6, by + 5, 10, Color{255, 215, 215, 240});
    }

    // ---- the top bar
    drawText(g.level().name, 14, 10, 20, kText);
    const std::string diffLabel = "[" + g.difficulty() + "] [D]";
    const int nw = MeasureText(g.level().name.c_str(), 20);
    DrawRectangle(18 + nw, 9, MeasureText(diffLabel.c_str(), 12) + 10, 20, Color{36, 40, 52, 220});
    DrawRectangleLines(18 + nw, 9, MeasureText(diffLabel.c_str(), 12) + 10, 20, kGrid);
    drawText(diffLabel, 23 + nw, 13, 12, (g.difficulty() == "nightmare" ? Color{255, 90, 80, 255} : (g.difficulty() == "hard" ? kGold : kTextDim)));

    // Titan Surge Tier Badge
    const int surgeTier = g.titanSurgeTier();
    if (surgeTier > 0) {
        const std::string surgeStr = titanSurgeName(surgeTier);
        const int tw = MeasureText(surgeStr.c_str(), 12);
        DrawRectangle(14, 34, tw + 14, 18, Color{160, 30, 45, 230});
        DrawRectangleLines(14, 34, tw + 14, 18, Color{255, 170, 70, 255});
        drawText(surgeStr, 21, 37, 12, Color{255, 235, 180, 255});
    }

    // Mode badges: Mute [M], Stats [S], Path [P], Endless [E], Auto-Wave [A], Speed [F]
    const int sndX = canvasW - 770;
    DrawRectangle(sndX, 8, 64, 24, audio_.isMuted() ? Color{80, 35, 35, 230} : Color{34, 38, 48, 200});
    DrawRectangleLines(sndX, 8, 64, 24, audio_.isMuted() ? kRed : kGrid);
    drawTextCentered(audio_.isMuted() ? "MUTE [M]" : "SND [M]", sndX + 32, 13, 11, audio_.isMuted() ? kRed : kTextDim);

    const int statsX = canvasW - 700;
    DrawRectangle(statsX, 8, 68, 24, view.showStatsModal ? Color{45, 65, 95, 230} : Color{34, 38, 48, 200});
    DrawRectangleLines(statsX, 8, 68, 24, view.showStatsModal ? Color{120, 200, 255, 255} : kGrid);
    drawTextCentered("STATS [S]", statsX + 34, 13, 11, view.showStatsModal ? Color{180, 220, 255, 255} : kTextDim);

    const int pathX = canvasW - 626;
    DrawRectangle(pathX, 8, 70, 24, view.showPathOverlay ? Color{40, 85, 75, 230} : Color{34, 38, 48, 200});
    DrawRectangleLines(pathX, 8, 70, 24, view.showPathOverlay ? Color{80, 230, 180, 255} : kGrid);
    drawTextCentered("PATH [P]", pathX + 35, 13, 11, view.showPathOverlay ? Color{140, 255, 220, 255} : kTextDim);

    const int spdX = canvasW - 550;
    DrawRectangle(spdX, 8, 88, 24, g.endlessMode() ? Color{75, 45, 95, 230} : Color{34, 38, 48, 200});
    DrawRectangleLines(spdX, 8, 88, 24, g.endlessMode() ? Color{210, 140, 255, 255} : kGrid);
    drawTextCentered("ENDLESS [E]", spdX + 44, 13, 11, g.endlessMode() ? Color{240, 200, 255, 255} : kTextDim);

    const int autoX = canvasW - 454;
    DrawRectangle(autoX, 8, 74, 24, g.autoWave() ? Color{35, 80, 50, 230} : Color{34, 38, 48, 200});
    DrawRectangleLines(autoX, 8, 74, 24, g.autoWave() ? Color{110, 230, 140, 255} : kGrid);
    drawTextCentered("AUTO [A]", autoX + 37, 13, 11, g.autoWave() ? Color{180, 255, 190, 255} : kTextDim);

    const int speedX = canvasW - 372;
    DrawRectangle(speedX, 8, 56, 24, g.gameSpeed() > 1 ? Color{90, 75, 30, 230} : Color{34, 38, 48, 200});
    DrawRectangleLines(speedX, 8, 56, 24, g.gameSpeed() > 1 ? kGold : kGrid);
    drawTextCentered(std::to_string(g.gameSpeed()) + "x [F]", speedX + 28, 13, 12, g.gameSpeed() > 1 ? kGold : kTextDim);

    drawScaled(sprites_.heart, canvasW - 300, 8, 24, Color{255, 255, 255, 255});
    drawText(std::to_string(g.lives()), canvasW - 272, 13, 18, kText);
    drawScaled(sprites_.coin, canvasW - 220, 8, 24, Color{255, 255, 255, 255});
    drawText(std::to_string(g.gold()), canvasW - 192, 13, 18, kGold);
    drawText("WAVE " + std::to_string(g.waveNumber()), canvasW - 130, 13, 18, kText);

    if (g.isWaveActive()) {
        std::string waveHeader = waveDef(g.waveNumber()).name;
        if (surgeTier > 0) waveHeader += " (" + std::to_string(static_cast<int>(std::round(std::pow(1.85f, surgeTier) * 100))) + "% HP)";
        drawTextCentered(waveHeader, canvasW / 2, 10, 16, surgeTier > 0 ? Color{255, 160, 120, 255} : kTextDim);
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

        // Target Priority Toggle
        DrawRectangle(sx + 14, y, sideW - 28, 24, kPanelLight);
        DrawRectangleLines(sx + 14, y, sideW - 28, 24, kGrid);
        std::string prioLabel = "TARGET: " + std::string(targetPriorityName(sel->targetPriority)) + "  [T]";
        drawTextCentered(prioLabel, sx + sideW / 2, y + 6, 11, kGold);
        y += 28;

        // Repair & Fortify button
        const bool canRepair = g.canRepair(sel->id);
        DrawRectangle(sx + 14, y, sideW - 28, 24, canRepair ? Color{32, 60, 48, 220} : kPanel);
        DrawRectangleLines(sx + 14, y, sideW - 28, 24, canRepair ? Color{70, 200, 140, 255} : kGrid);
        std::string repLabel = "REPAIR & SHIELD (20g) [R]";
        if (sel->shieldHp >= 50 && sel->hp >= sel->maxHp) repLabel = "FORTIFIED (100% + Shield)";
        drawTextCentered(repLabel, sx + sideW / 2, y + 6, 11, canRepair ? Color{140, 240, 180, 255} : kTextDim);
        y += 28;

        // Overdrive button for Lv3
        if (sel->level >= 3) {
            const bool canOd = g.canOverdrive(sel->id);
            DrawRectangle(sx + 14, y, sideW - 28, 24, canOd ? Color{70, 55, 25, 220} : kPanel);
            DrawRectangleLines(sx + 14, y, sideW - 28, 24, canOd ? kGold : kGrid);
            std::string odLabel = "OVERDRIVE (45g) [O]";
            if (sel->overdriveRemaining > 0) odLabel = "OVERDRIVE: " + std::to_string(static_cast<int>(std::ceil(sel->overdriveRemaining))) + "s";
            drawTextCentered(odLabel, sx + sideW / 2, y + 6, 11, canOd ? kGold : (sel->overdriveRemaining > 0 ? Color{255, 220, 80, 255} : kTextDim));
            y += 28;
        }

        if (sel->level < def.maxLevel()) {
            const int cost = def.level(sel->level).cost;
            const bool can = g.gold() >= cost;
            DrawRectangle(sx + 14, y, sideW - 28, 26, can ? kPanelLight : kPanel);
            drawText("UPGRADE  (" + std::to_string(cost) + "g)  [U]", sx + 22, y + 6, 12,
                     can ? kGold : kTextDim);
        } else {
            drawText("MAX LEVEL", sx + 14, y + 6, 13, kTextDim);
        }
        y += 30;
        DrawRectangle(sx + 14, y, sideW - 28, 26, kPanelLight);
        drawText("SELL  [X]", sx + 22, y + 6, 13, kText);
    } else {
        // ---- Wave Intel Preview when no tower is selected
        y += 8;
        DrawRectangle(sx + 10, y, sideW - 20, 2, kGrid);
        y += 10;
        const int nextWaveNum = g.isWaveActive() ? (g.waveNumber() + 1) : std::max(1, g.waveNumber() + 1);
        const WaveDef& nextW = waveDef(nextWaveNum);
        const int nextTier = titanSurgeTier(nextWaveNum);

        drawText("UPCOMING: " + nextW.name, sx + 14, y, 14, nextTier > 0 ? Color{255, 140, 100, 255} : kGold);
        y += 18;

        int totalCount = 0;
        for (const auto& grp : nextW.groups) totalCount += grp.count;
        drawText("Horde Strength: " + std::to_string(totalCount) + " enemies", sx + 14, y, 12, kTextDim);
        y += 16;

        if (nextTier > 0) {
            std::string surgeDetail = "Surge " + std::to_string(nextTier) + " (" +
                                      std::to_string(static_cast<int>(std::round(std::pow(1.85f, nextTier) * 100))) + "% HP, +" +
                                      std::to_string(nextTier * 2) + " Armor)";
            drawText(surgeDetail, sx + 14, y, 11, Color{255, 180, 80, 255});
            y += 15;
        }

        // Summary of groups
        for (size_t i = 0; i < nextW.groups.size() && i < 3; ++i) {
            const auto& grp = nextW.groups[i];
            std::string gDesc = std::to_string(grp.count) + "x " + enemyDef(grp.kind).name;
            drawText(gDesc, sx + 20, y, 12, kTextDim);
            y += 15;
        }
    }

    // ---- Commander Tactical Action Bar (Bottom of canvas)
    const int actionY = screenH - 38;
    const int btnW = 125;
    const int btnH = 28;
    const int startBtnX = 14;

    auto drawAbilityBtn = [&](int idx, const char* name, int cost, CommanderAbility ab, Color activeCol) {
        const int bx = startBtnX + idx * (btnW + 10);
        const float cd = g.abilityCooldown(ab);
        const bool can = g.canCastAbility(ab);

        DrawRectangle(bx, actionY, btnW, btnH, can ? Color{36, 42, 54, 230} : Color{24, 26, 32, 220});
        DrawRectangleLines(bx, actionY, btnW, btnH, can ? activeCol : kGrid);

        if (cd > 0.0f) {
            float cdFrac = cd / g.abilityMaxCooldown(ab);
            DrawRectangle(bx, actionY, static_cast<int>(btnW * cdFrac), btnH, Color{15, 15, 20, 160});
            std::string cdText = std::string(name) + " (" + std::to_string(static_cast<int>(std::ceil(cd))) + "s)";
            drawTextCentered(cdText, bx + btnW / 2, actionY + 7, 11, Color{220, 140, 140, 255});
        } else {
            drawText(name, bx + 8, actionY + 3, 11, can ? kText : kTextDim);
            drawText(std::to_string(cost) + "g", bx + 8, actionY + 14, 10, can ? kGold : Color{140, 120, 70, 255});
        }
    };

    drawAbilityBtn(0, "[1] METEOR", g.abilityCost(CommanderAbility::Meteor), CommanderAbility::Meteor, Color{255, 130, 50, 255});
    drawAbilityBtn(1, "[2] CRYO", g.abilityCost(CommanderAbility::CryoFreeze), CommanderAbility::CryoFreeze, Color{80, 210, 255, 255});
    drawAbilityBtn(2, "[3] MIDAS", g.abilityCost(CommanderAbility::MidasRush), CommanderAbility::MidasRush, Color{245, 200, 60, 255});

    // ---- the status message
    if (!g.statusMessage().empty()) {
        const std::string& m = g.statusMessage();
        const int w = MeasureText(m.c_str(), 15);
        DrawRectangle(canvasW / 2 - w / 2 - 12, screenH - 72, w + 24, 26, Color{20, 22, 28, 200});
        drawTextCentered(m, canvasW / 2, screenH - 67, 15, kText);
    }

    // ---- the end screens
    if (g.isGameOver()) {
        DrawRectangle(0, 0, screenW, screenH, Color{10, 8, 12, 210});
        drawTextCentered("THE CORE WAS DESTROYED", screenW / 2, screenH / 2 - 80, 40, kRed);
        drawTextCentered("Wave " + std::to_string(g.waveNumber()) + "   ·   " +
                         std::to_string(g.kills()) + " kills   ·   " +
                         std::to_string(g.score()) + " points",
                         screenW / 2, screenH / 2 - 30, 20, kText);
        if (g.titanSurgeTier() > 0) {
            drawTextCentered("Survived to " + std::string(titanSurgeName(g.titanSurgeTier())),
                             screenW / 2, screenH / 2, 18, Color{255, 160, 100, 255});
        }
        drawTextCentered("Press R to try again   ·   [S] Combat Metrics", screenW / 2, screenH / 2 + 35, 20, kTextDim);
    } else if (g.isVictory()) {
        DrawRectangle(0, 0, screenW, screenH, Color{10, 12, 16, 210});
        drawTextCentered("ALL CAMPAIGN WAVES CLEARED", screenW / 2, screenH / 2 - 90, 38, kGold);
        drawTextCentered(std::to_string(g.kills()) + " kills   ·   " +
                         std::to_string(g.score()) + " points   ·   " +
                         std::to_string(g.lives()) + " lives left",
                         screenW / 2, screenH / 2 - 40, 20, kText);
        drawTextCentered("Press [E] to enter ENDLESS TITAN ESCALATION", screenW / 2, screenH / 2 + 10, 20, Color{230, 180, 255, 255});
        drawTextCentered("Press [R] to restart campaign   ·   [S] Combat Metrics", screenW / 2, screenH / 2 + 45, 18, kTextDim);
    }

    // ---- Combat Metrics & Titan Debrief Modal
    if (view.showStatsModal) {
        const int mw = 680;
        const int mh = 480;
        const int mx = (screenW - mw) / 2;
        const int my = (screenH - mh) / 2;

        DrawRectangle(0, 0, screenW, screenH, Color{10, 12, 16, 220});
        DrawRectangle(mx, my, mw, mh, kPanel);
        DrawRectangleLines(mx, my, mw, mh, kGrid);
        DrawRectangle(mx, my, mw, 38, kPanelLight);

        drawTextCentered("COMBAT METRICS & TITAN DEBRIEF", mx + mw / 2, my + 10, 18, kGold);

        const auto& st = g.stats();
        const int col1X = mx + 25;
        const int col2X = mx + 340;
        int curY = my + 52;

        // Left Column: Campaign & Economy Summary
        drawText("OPERATION SUMMARY", col1X, curY, 14, kTextDim);
        curY += 24;
        drawText("Difficulty: " + g.difficulty(), col1X, curY, 13, kText);
        curY += 20;
        drawText("Waves Conquered: " + std::to_string(g.waveNumber()), col1X, curY, 13, kText);
        curY += 20;
        std::string peakSurge = (st.maxSurgeTier > 0) ? titanSurgeName(st.maxSurgeTier) : "Baseline";
        drawText("Peak Titan Surge: " + peakSurge, col1X, curY, 13, (st.maxSurgeTier > 0) ? Color{255, 160, 100, 255} : kText);
        curY += 20;
        drawText("Titans Felled: " + std::to_string(st.titansDefeated), col1X, curY, 14, (st.titansDefeated > 0) ? kGold : kText);
        curY += 20;
        drawText("Enemies Eliminated: " + std::to_string(st.totalEnemiesKilled), col1X, curY, 13, kText);
        curY += 20;
        drawText("Total Damage Dealt: " + std::to_string(st.totalDamageDealt), col1X, curY, 13, kText);
        curY += 20;
        drawText("Gold Earned: " + std::to_string(st.totalGoldEarned) + "g", col1X, curY, 13, kGold);
        curY += 20;
        drawText("Gold Invested: " + std::to_string(st.totalGoldSpent) + "g", col1X, curY, 13, kTextDim);
        curY += 20;
        drawText("Towers Built / Lost: " + std::to_string(st.towersBuilt) + " / " + std::to_string(st.towersLost), col1X, curY, 13, (st.towersLost > 0) ? kRed : kText);

        // Right Column: Tower DPS Breakdown
        curY = my + 52;
        drawText("TOWER DAMAGE SHARE", col2X, curY, 14, kTextDim);
        curY += 24;

        uint64_t maxKindDmg = 1;
        size_t mvpIdx = 0;
        for (size_t i = 0; i < 12; ++i) {
            if (st.damageByTowerKind[i] > maxKindDmg) {
                maxKindDmg = st.damageByTowerKind[i];
                mvpIdx = i;
            }
        }

        int renderedBars = 0;
        for (size_t i = 0; i < 12 && renderedBars < 9; ++i) {
            if (st.damageByTowerKind[i] == 0) continue;
            TowerKind kind = static_cast<TowerKind>(i);
            const TowerDef& def = towerDef(kind);
            float frac = static_cast<float>(st.damageByTowerKind[i]) / static_cast<float>(maxKindDmg);
            int barW = static_cast<int>(frac * 140.0f);

            std::string label = def.name;
            if (i == mvpIdx && st.totalDamageDealt > 0) label += " [MVP]";
            drawText(label, col2X, curY, 12, (i == mvpIdx) ? kGold : kText);

            DrawRectangle(col2X + 110, curY + 2, 140, 10, Color{40, 44, 56, 255});
            DrawRectangle(col2X + 110, curY + 2, barW, 10, (i == mvpIdx) ? kGold : Color{110, 180, 240, 255});
            drawText(std::to_string(st.damageByTowerKind[i]), col2X + 258, curY, 11, kTextDim);

            curY += 20;
            renderedBars++;
        }
        if (renderedBars == 0) {
            drawText("No tower damage recorded yet.", col2X, curY, 12, kTextDim);
        }

        // Bottom prompt
        drawTextCentered("Press [S] or [ESC] to return to simulation", mx + mw / 2, my + mh - 28, 14, kGold);
    }
}

}  // namespace tf
