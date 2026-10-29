#include "core/Game.h"

#include <algorithm>
#include <cmath>

namespace tf {

// ------------------------------------------------------------------ levels

namespace {

// Draws a straight line of path tiles between two waypoints (Bresenham-ish).
void markSegment(Grid* g, Cell a, Cell b) {
    const int dx = b.x - a.x;
    const int dy = b.y - a.y;
    const int steps = std::max(std::abs(dx), std::abs(dy));
    if (steps == 0) {
        const Terrain existing = g->at(a.x, a.y);
        if (existing != Terrain::Rock && existing != Terrain::Water) {
            g->set(a.x, a.y, Terrain::Path);
        }
        return;
    }
    for (int i = 0; i <= steps; ++i) {
        const float t = static_cast<float>(i) / steps;
        const int x = a.x + static_cast<int>(std::lround(dx * t));
        const int y = a.y + static_cast<int>(std::lround(dy * t));
        // Never overwrite rock or water: those are terrain features, not path.
        const Terrain existing = g->at(x, y);
        if (existing == Terrain::Rock || existing == Terrain::Water) continue;
        g->set(x, y, Terrain::Path);
    }
}

}  // namespace

void LevelDef::applyRoute(Grid* grid) const {
    if (!grid || waypoints.size() < 2) return;
    for (size_t i = 0; i + 1 < waypoints.size(); ++i) {
        markSegment(grid, waypoints[i], waypoints[i + 1]);
    }
}

const std::vector<LevelDef>& allLevels() {
    static const std::vector<LevelDef> levels = [] {
        std::vector<LevelDef> v;

        // ---- Level 1: "Meadow Run"
        //     A wide field with two rock spines. The route drops down column 1,
        //     runs along the bottom, and comes back up at column 15. The rock
        //     spines make the vertical legs ONE TILE WIDE — a genuine
        //     chokepoint the player will want to seal.
        {
            LevelDef l;
            l.name = "Meadow Run";
            l.difficulty = "easy";
            l.width = 24; l.height = 14;
            l.rows = {
                "########################",
                "#......................#",
                "#.###............###...#",
                "#.###............###...#",
                "#......................#",
                "#......................#",
                "S......................E",
                "#......................#",
                "#......................#",
                "#.###............###...#",
                "#.###............###...#",
                "#......................#",
                "#......................#",
                "########################",
            };
            l.waypoints = {
                Cell{0, 6}, Cell{1, 6}, Cell{1, 11}, Cell{15, 11}, Cell{15, 6}, Cell{23, 6},
            };
            v.push_back(l);
        }

        // ---- Level 2: a switchback with a central plaza.
        {
            LevelDef l;
            l.name = "Stone Pass";
            l.difficulty = "normal";
            l.width = 24; l.height = 14;
            l.rows = {
                "########################",
                "#......................#",
                "#....~~~~......~~~~....#",
                "#....~~~~......~~~~....#",
                "#......................#",
                "#..##..............##..#",
                "S..##....######....##..E",
                "#..##....######....##..#",
                "#......................#",
                "#......................#",
                "#....~~~~......~~~~....#",
                "#....~~~~......~~~~....#",
                "#......................#",
                "########################",
            };
            // The central stone block covers x=9..14 on rows 6-7, and the water
            // pools cover x=15..18 on rows 10-11, so the route steps around both:
            // up column 14 to row 8, across one tile to column 15, then up.
            l.waypoints = {
                Cell{0, 6}, Cell{1, 6}, Cell{1, 12}, Cell{14, 12}, Cell{14, 8},
                Cell{15, 8}, Cell{15, 4}, Cell{21, 4}, Cell{21, 6}, Cell{23, 6},
            };
            v.push_back(l);
        }

        // ---- Level 3: a long serpentine with tight pockets.
        {
            LevelDef l;
            l.name = "Serpent Hollow";
            l.difficulty = "hard";
            l.width = 26; l.height = 16;
            l.rows = {
                "##########################",
                "#........................#",
                "#..####..........####....#",
                "#..####..........####....#",
                "#........................#",
                "#.....##........##.......#",
                "S.....##........##.......E",
                "#.....##........##.......#",
                "#........................#",
                "#........................#",
                "#....####........####....#",
                "#....####........####....#",
                "#........................#",
                "#........................#",
                "#........................#",
                "##########################",
            };
            l.waypoints = {
                Cell{0, 6}, Cell{1, 6}, Cell{1, 14}, Cell{12, 14}, Cell{12, 1},
                Cell{24, 1}, Cell{24, 6}, Cell{25, 6},
            };
            v.push_back(l);
        }

        return v;
    }();
    return levels;
}

}  // namespace tf
