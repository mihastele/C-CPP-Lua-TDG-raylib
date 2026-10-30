// towerfall — the game loop and input handling.
//
// The simulation lives in tfcore; this file wires it to raylib: it reads the
// mouse and keyboard, asks the Game to do things, and draws the result.

#include <cmath>
#include <cstdio>
#include <string>

#include "core/Game.h"
#include "core/LuaContent.h"
#include "raylib.h"
#include "render/Renderer.h"

using namespace tf;

namespace {

constexpr int kWindowW = 1280;
constexpr int kWindowH = 720;
constexpr int kSideW = 300;

struct App {
    Game game;
    GameView view;
    Renderer renderer;
    int levelIndex = 0;
    bool showHelp = false;
};

void loadLevel(App& app, int index) {
    app.levelIndex = ((index % levelCount()) + levelCount()) % levelCount();
    app.game.loadLevel(levelByIndex(app.levelIndex));
    app.view.selectedTowerId = 0;
    app.view.mode = ToolMode::Select;
}

/** Maps a mouse position to a grid cell, or {-1,-1} if it is off the map. */
Cell mouseToCell(const Game& game, Vector2 mouse) {
    const int canvasW = GetScreenWidth() - kSideW;
    const int ts = std::min(static_cast<int>(canvasW / game.grid().width()),
                            static_cast<int>((GetScreenHeight() - 40) / game.grid().height()));
    const int mapW = ts * game.grid().width();
    const int mapH = ts * game.grid().height();
    const int mapX = (canvasW - mapW) / 2;
    const int mapY = 40 + (GetScreenHeight() - 40 - mapH) / 2;

    const MapView view{static_cast<float>(mapX), static_cast<float>(mapY),
                       static_cast<float>(ts), game.tileSize()};
    const Cell c = view.screenToCell(mouse.x, mouse.y);
    if (!game.grid().inBounds(c.x, c.y)) return Cell{-1, -1};
    return c;
}

Vec2 mouseToWorld(const Game& game, Vector2 mouse) {
    const int canvasW = GetScreenWidth() - kSideW;
    const int ts = std::min(static_cast<int>(canvasW / game.grid().width()),
                            static_cast<int>((GetScreenHeight() - 40) / game.grid().height()));
    const int mapW = ts * game.grid().width();
    const int mapH = ts * game.grid().height();
    const int mapX = (canvasW - mapW) / 2;
    const int mapY = 40 + (GetScreenHeight() - 40 - mapH) / 2;

    const MapView view{static_cast<float>(mapX), static_cast<float>(mapY),
                       static_cast<float>(ts), game.tileSize()};
    return view.screenToWorld(mouse.x, mouse.y);
}

/** Where the i-th tower button starts in the sidebar. */
int towerButtonY(int index) { return 36 + index * 46; }

void drawTextCentered(const std::string& s, int cx, int y, int size, Color c) {
    const int w = MeasureText(s.c_str(), size);
    DrawText(s.c_str(), cx - w / 2, y, size, c);
}

}  // namespace

int main() {
    // ---- load the content from Lua
    const LuaContent content = loadLuaContent(TF_CONTENT_DIR "/content.lua");
    if (!content.ok()) {
        std::printf("towerfall: content problems:\n");
        for (const auto& e : content.errors) std::printf("  - %s\n", e.c_str());
        std::printf("continuing with the built-in content\n");
    }

    SetTraceLogLevel(LOG_WARNING);
    InitWindow(kWindowW, kWindowH, "Towerfall — a tower defence game");
    SetTargetFPS(60);
    SetExitKey(KEY_NULL);      // the game handles its own quit key

    App app;
    loadLevel(app, 0);
    app.renderer.init();

    while (!WindowShouldClose()) {
        const float dt = std::min(GetFrameTime(), 0.05f);
        const Vector2 mouse = GetMousePosition();
        const int canvasW = GetScreenWidth() - kSideW;

        // ---- top bar control buttons
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && mouse.y >= 8 && mouse.y <= 32) {
            if (mouse.x >= 14 && mouse.x <= 200) {
                app.game.cycleDifficulty();
            } else if (mouse.x >= canvasW - 770 && mouse.x <= canvasW - 706) {
                app.renderer.audio().toggleMute();
            } else if (mouse.x >= canvasW - 700 && mouse.x <= canvasW - 632) {
                app.view.showStatsModal = !app.view.showStatsModal;
            } else if (mouse.x >= canvasW - 626 && mouse.x <= canvasW - 556) {
                app.view.showPathOverlay = !app.view.showPathOverlay;
            } else if (mouse.x >= canvasW - 550 && mouse.x <= canvasW - 462) {
                app.game.toggleEndlessMode();
            } else if (mouse.x >= canvasW - 454 && mouse.x <= canvasW - 380) {
                app.game.toggleAutoWave();
            } else if (mouse.x >= canvasW - 372 && mouse.x <= canvasW - 316) {
                app.game.cycleGameSpeed();
            }
        }

        // ---- the sidebar's tower buttons
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && mouse.x >= canvasW) {
            for (size_t i = 0; i < allTowerDefs().size(); ++i) {
                const int by = towerButtonY(static_cast<int>(i));
                if (mouse.y >= by && mouse.y < by + 42) {
                    app.view.mode = ToolMode::Build;
                    app.view.buildKind = allTowerDefs()[i].kind;
                    app.view.selectedTowerId = 0;
                    break;
                }
            }
        }

        // ---- commander ability bar clicks
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && mouse.y >= GetScreenHeight() - 40 && mouse.x < canvasW) {
            const int btnW = 125;
            for (int i = 0; i < 3; ++i) {
                const int bx = 14 + i * (btnW + 10);
                if (mouse.x >= bx && mouse.x < bx + btnW) {
                    if (i == 0) app.view.targetingMeteor = !app.view.targetingMeteor;
                    else if (i == 1) app.game.castCryoFreeze();
                    else if (i == 2) app.game.castMidasRush();
                    break;
                }
            }
        }

        // ---- clicking the map
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && mouse.x < canvasW && mouse.y < GetScreenHeight() - 40 && mouse.y > 34) {
            const Vec2 wpos = mouseToWorld(app.game, mouse);
            if (app.view.targetingMeteor) {
                if (app.game.castMeteor(wpos)) {
                    app.view.targetingMeteor = false;
                }
            } else if (app.game.collectPowerupAt(wpos, 28.0f)) {
                // Clicked and collected powerup orb!
            } else {
                const Cell cell = mouseToCell(app.game, mouse);
                if (cell.x >= 0) {
                    if (app.view.mode == ToolMode::Build) {
                        if (app.game.build(app.view.buildKind, cell)) {
                            app.view.selectedTowerId = 0;
                            // The build mode STAYS ON, so a tower can be placed repeatedly.
                            // Right-click or ESC cancels it.
                        }
                    } else {
                        // Select a tower, or deselect.
                        app.view.selectedTowerId = 0;
                        for (const auto& t : app.game.towers()) {
                            if (t.cell == cell) { app.view.selectedTowerId = t.id; break; }
                        }
                    }
                }
            }
        }

        // ---- right-click cancels
        if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
            if (app.view.targetingMeteor) app.view.targetingMeteor = false;
            else {
                app.view.mode = ToolMode::Select;
                app.view.selectedTowerId = 0;
            }
        }

        // ---- keyboard
        if (IsKeyPressed(KEY_ESCAPE)) {
            if (app.view.targetingMeteor) app.view.targetingMeteor = false;
            else if (app.view.showStatsModal) app.view.showStatsModal = false;
            else if (app.showHelp) app.showHelp = false;
            else if (app.view.mode == ToolMode::Build) app.view.mode = ToolMode::Select;
            else app.view.selectedTowerId = 0;
        }
        if (IsKeyPressed(KEY_Q) || IsKeyPressed(KEY_ONE)) app.view.targetingMeteor = !app.view.targetingMeteor;
        if (IsKeyPressed(KEY_W) || IsKeyPressed(KEY_TWO)) app.game.castCryoFreeze();
        if (IsKeyPressed(KEY_Z) || IsKeyPressed(KEY_THREE)) app.game.castMidasRush();
        if (IsKeyPressed(KEY_SPACE)) app.game.startWaveNow();
        if (IsKeyPressed(KEY_F)) app.game.cycleGameSpeed();
        if (IsKeyPressed(KEY_E)) app.game.toggleEndlessMode();
        if (IsKeyPressed(KEY_A)) app.game.toggleAutoWave();
        if (IsKeyPressed(KEY_D)) app.game.cycleDifficulty();
        if (IsKeyPressed(KEY_P)) app.view.showPathOverlay = !app.view.showPathOverlay;
        if (IsKeyPressed(KEY_S)) app.view.showStatsModal = !app.view.showStatsModal;
        if (IsKeyPressed(KEY_M)) app.renderer.audio().toggleMute();
        if (IsKeyPressed(KEY_T) && app.view.selectedTowerId != 0) {
            app.game.cycleTowerTargetPriority(app.view.selectedTowerId);
        }
        if (IsKeyPressed(KEY_R)) {
            if (app.view.selectedTowerId != 0) {
                app.game.repair(app.view.selectedTowerId);
            } else {
                loadLevel(app, app.levelIndex);
            }
        }
        if (IsKeyPressed(KEY_O) && app.view.selectedTowerId != 0) {
            app.game.overdrive(app.view.selectedTowerId);
        }
        if (IsKeyPressed(KEY_U) && app.view.selectedTowerId != 0) {
            app.game.upgrade(app.view.selectedTowerId);
        }
        if (IsKeyPressed(KEY_X) && app.view.selectedTowerId != 0) {
            app.game.sell(app.view.selectedTowerId);
            app.view.selectedTowerId = 0;
        }
        if (IsKeyPressed(KEY_TAB)) loadLevel(app, app.levelIndex + 1);
        if (IsKeyPressed(KEY_H)) app.showHelp = !app.showHelp;
        // Number keys 4..9 pick towers.
        for (int i = 3; i < 10 && i < static_cast<int>(allTowerDefs().size()); ++i) {
            if (IsKeyPressed(KEY_ONE + i)) {
                app.view.mode = ToolMode::Build;
                app.view.buildKind = allTowerDefs()[static_cast<size_t>(i)].kind;
                app.view.selectedTowerId = 0;
            }
        }

        // ---- simulate
        app.view.hover = mouseToCell(app.game, mouse);
        app.view.time += dt;
        app.game.update(dt);

        // ---- draw
        BeginDrawing();
        ClearBackground(Color{22, 24, 30, 255});
        app.renderer.draw(app.game, app.view);

        if (app.showHelp) {
            const int sw = GetScreenWidth();
            const int sh = GetScreenHeight();
            DrawRectangle(0, 0, sw, sh, Color{10, 12, 16, 225});
            const int cx = sw / 2;
            drawTextCentered("TOWERFALL", cx, 65, 42, Color{246, 200, 82, 255});
            drawTextCentered("Build anywhere without sealing the route · Unleash commander abilities", cx, 115, 18,
                             Color{146, 154, 170, 255});
            int hy = 155;
            const char* lines[] = {
                "LEFT CLICK a tower in sidebar, then click map to build",
                "Towers may be built ANYWHERE — including on the path, but never seal it completely",
                "",
                "COMMANDER POWERS:  [1]/[Q] Orbital Meteor (35g)    [2]/[W] Cryo Freeze (40g)    [3]/[Z] Midas Rush (45g)",
                "POWERUPS: Click glowing orbs dropped by Titans & Elites (+30g, Overcharge All, Freeze, Core Life)",
                "ELEMENTAL COMBOS: Burn + Frost = THERMAL SHOCK · Poison + Tesla = SUPERCONDUCTOR · Sniper = SHATTER",
                "",
                "LEFT CLICK a built tower: [U] upgrade    [X] sell    [T] target priority",
                "[R] repair & fortify shield (20g)       [O] overdrive supercharge (45g)",
                "[P] path & threat heatmap overlay       [S] combat stats & debrief       [M] mute audio",
                "[F] game speed (1x/2x/3x)               [A] auto-wave                    [E] endless titan mode",
                "[D] difficulty                          SPACE call wave early            TAB next level",
                "",
                "TITAN SURGE: Every 4 waves after Wave 5, the horde scales EXPONENTIALLY in HP, speed & siege!",
                "Titans possess Earthquake Stomp & Cleave Slam! Elite Affixes: Frenzied, Juggernaut, Shielded...",
            };
            for (const char* l : lines) {
                drawTextCentered(l, cx, hy, 16, Color{210, 216, 228, 255});
                hy += 25;
            }
        }
        EndDrawing();
    }

    app.renderer.shutdown();
    CloseWindow();
    return 0;
}
