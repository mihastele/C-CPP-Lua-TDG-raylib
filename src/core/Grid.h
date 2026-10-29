// towerfall — the grid, and the rules that govern where things can go.
//
// The central design rule from the design brief: a tower may be placed ANYWHERE,
// including on the path itself — but it may never make the path impassable. That
// means placement has to be validated against the pathfinding result, not against
// a hard-coded "path tiles are protected" rule.
//
// The check is therefore: "if this tower were here, could an enemy still get from
// the entry to the exit?" That is answered by re-running the pathfinder with the
// tower's cells treated as blocked.

#pragma once

#include <string>
#include <vector>

#include "core/Types.h"

namespace tf {

class Grid {
public:
    Grid() = default;
    Grid(int w, int h) { resize(w, h); }

    void resize(int w, int h);

    int width() const { return w_; }
    int height() const { return h_; }
    bool inBounds(int x, int y) const { return x >= 0 && y >= 0 && x < w_ && y < h_; }
    bool inBounds(Cell c) const { return inBounds(c.x, c.y); }

    Terrain at(int x, int y) const;
    Terrain at(Cell c) const { return at(c.x, c.y); }
    void set(int x, int y, Terrain t);
    void set(Cell c, Terrain t) { set(c.x, c.y, t); }

    // ---- buildability
    //
    // Terrain must be buildable AND no tower may already occupy the cell. Path
    // tiles ARE buildable — that is the whole point of the brief.
    bool isBuildableTerrain(int x, int y) const;
    bool isBuildableTerrain(Cell c) const { return isBuildableTerrain(c.x, c.y); }

    // ---- occupancy (towers block movement but do not change terrain)
    void setTower(Cell c, int towerId);      // towerId == 0 clears
    int towerAt(Cell c) const;
    bool hasTower(Cell c) const { return towerAt(c) != 0; }
    void clearTowers();

    // ---- the pathfinding query used everywhere
    //
    // Finds a route from `from` to `to` treating towers as blockers. Returns an
    // empty path when no route exists.
    //
    // `ignoreTowers` is used to compute the "what if there were no towers" route
    // that some enemy behaviours need.
    std::vector<Cell> findPath(Cell from, Cell to, bool ignoreTowers = false) const;

    // Is `to` reachable from `from`? Used to validate a hypothetical placement.
    bool canReach(Cell from, Cell to, bool ignoreTowers = false) const;

    // The tiles the intended path runs through, purely for drawing.
    std::vector<Cell> pathTiles() const { return pathTiles_; }
    void setPathTiles(std::vector<Cell> t) { pathTiles_ = std::move(t); }

    // ---- level parsing
    //
    // Builds the terrain from ASCII rows. Characters:
    //   '#' rock (blocked)   '~' water (blocked)   '.' grass   ',' sand
    //   'S' start            'E' exit               otherwise path
    static Grid fromAscii(const std::vector<std::string>& rows,
                          Cell* startOut = nullptr,
                          Cell* endOut = nullptr);

    // ---- level authoring helper: a serpentine path from left to right
    static std::vector<Cell> serpentinePath(int w, int h, int margin = 1);

private:
    int w_ = 0, h_ = 0;
    std::vector<Terrain> terrain_;
    std::vector<int> towers_;          // 0 = empty, else tower id
    std::vector<Cell> pathTiles_;
};

// Cost of stepping onto a tile. Path tiles are cheaper so that enemies follow the
// intended route when there is a choice, but they are NOT forbidden — enemies
// happily walk over grass when the path is blocked.
int terrainStepCost(Terrain t);

// A* over the 4-neighbourhood. `blocked` is consulted in addition to terrain.
std::vector<Cell> astar(const Grid& grid, Cell from, Cell to,
                        const std::vector<int>& towers,
                        bool ignoreTowers);

}  // namespace tf
