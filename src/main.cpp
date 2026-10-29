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

    // The SAME conversion the renderer uses, so a click always lands on the tile
    // that is actually drawn under the cursor.
    const MapView view{static_cast<float>(mapX), static_cast<float>(mapY),
                       static_cast<float>(ts), game.tileSize()};
    const Cell c = view.screenToCell(mouse.x, mouse.y);
    if (!game.grid().inBounds(c.x, c.y)) return Cell{-1, -1};
    return c;
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

        // ---- clicking the map
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && mouse.x < canvasW) {
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

        // ---- right-click cancels
        if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
            app.view.mode = ToolMode::Select;
            app.view.selectedTowerId = 0;
        }

        // ---- keyboard
        if (IsKeyPressed(KEY_ESCAPE)) {
            if (app.view.mode == ToolMode::Build) app.view.mode = ToolMode::Select;
            else app.view.selectedTowerId = 0;
        }
        if (IsKeyPressed(KEY_SPACE)) app.game.startWaveNow();
        if (IsKeyPressed(KEY_U) && app.view.selectedTowerId != 0) {
            app.game.upgrade(app.view.selectedTowerId);
        }
        if (IsKeyPressed(KEY_X) && app.view.selectedTowerId != 0) {
            app.game.sell(app.view.selectedTowerId);
            app.view.selectedTowerId = 0;
        }
        if (IsKeyPressed(KEY_TAB)) loadLevel(app, app.levelIndex + 1);
        if (IsKeyPressed(KEY_R)) loadLevel(app, app.levelIndex);
        if (IsKeyPressed(KEY_H)) app.showHelp = !app.showHelp;
        if (IsKeyPressed(KEY_ESCAPE) && app.showHelp) app.showHelp = false;
        // Number keys pick a tower.
        for (int i = 0; i < 10 && i < static_cast<int>(allTowerDefs().size()); ++i) {
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
            drawTextCentered("TOWERFALL", cx, 80, 44, Color{246, 200, 82, 255});
            drawTextCentered("Build towers to stop the horde reaching your core.", cx, 140, 20,
                             Color{146, 154, 170, 255});
            int hy = 190;
            const char* lines[] = {
                "LEFT CLICK a tower in the sidebar, then click the map to build it",
                "Towers may be built ANYWHERE — including on the path",
                "…but you may never block the path completely; the game refuses",
                "",
                "LEFT CLICK a built tower to select it     [U] upgrade     [X] sell",
                "SPACE  call the next wave early",
                "TAB    next level      R  restart      H  this help",
                "",
                "Sappers and Bruisers ignore the path and attack your towers directly.",
                "Flyers ignore everything and fly straight to the core.",
                "Bosses arrive every fifth wave.",
            };
            for (const char* l : lines) {
                drawTextCentered(l, cx, hy, 17, Color{210, 216, 228, 255});
                hy += 27;
            }
        }
        EndDrawing();
    }

    app.renderer.shutdown();
    CloseWindow();
    return 0;
}
