#include "core/Grid.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <queue>

namespace tf {

float Vec2::length() const { return std::sqrt(x * x + y * y); }
float Vec2::lengthSquared() const { return x * x + y * y; }
float Vec2::distanceTo(const Vec2& o) const {
    const float dx = x - o.x, dy = y - o.y;
    return std::sqrt(dx * dx + dy * dy);
}
Vec2 Vec2::normalized() const {
    const float l = length();
    return l > 1e-6f ? Vec2{x / l, y / l} : Vec2{0, 0};
}

// ------------------------------------------------------------------ grid

void Grid::resize(int w, int h) {
    w_ = std::max(1, w);
    h_ = std::max(1, h);
    terrain_.assign(static_cast<size_t>(w_) * h_, Terrain::Grass);
    towers_.assign(static_cast<size_t>(w_) * h_, 0);
}

Terrain Grid::at(int x, int y) const {
    if (!inBounds(x, y)) return Terrain::Rock;
    return terrain_[static_cast<size_t>(y) * w_ + x];
}

void Grid::set(int x, int y, Terrain t) {
    if (!inBounds(x, y)) return;
    terrain_[static_cast<size_t>(y) * w_ + x] = t;
}

bool Grid::isBuildableTerrain(int x, int y) const {
    const Terrain t = at(x, y);
    return t == Terrain::Grass || t == Terrain::Path || t == Terrain::Sand;
}

void Grid::setTower(Cell c, int towerId) {
    if (!inBounds(c)) return;
    towers_[static_cast<size_t>(c.y) * w_ + c.x] = towerId;
}

int Grid::towerAt(Cell c) const {
    if (!inBounds(c)) return 0;
    return towers_[static_cast<size_t>(c.y) * w_ + c.x];
}

void Grid::clearTowers() {
    std::fill(towers_.begin(), towers_.end(), 0);
}

Grid Grid::fromAscii(const std::vector<std::string>& rows, Cell* startOut, Cell* endOut) {
    const int h = static_cast<int>(rows.size());
    const int w = h > 0 ? static_cast<int>(rows[0].size()) : 0;
    Grid g(w, h);

    for (int y = 0; y < h; ++y) {
        const std::string& row = rows[static_cast<size_t>(y)];
        for (int x = 0; x < static_cast<int>(row.size()); ++x) {
            const char ch = row[static_cast<size_t>(x)];
            switch (ch) {
                case '#': g.set(x, y, Terrain::Rock); break;
                case '~': g.set(x, y, Terrain::Water); break;
                case ',': g.set(x, y, Terrain::Sand); break;
                case '.': g.set(x, y, Terrain::Grass); break;
                case 'S':
                    g.set(x, y, Terrain::Path);
                    if (startOut) *startOut = Cell{x, y};
                    break;
                case 'E':
                    g.set(x, y, Terrain::Path);
                    if (endOut) *endOut = Cell{x, y};
                    break;
                default:
                    g.set(x, y, Terrain::Path);
                    break;
            }
        }
    }
    return g;
}

std::vector<Cell> Grid::serpentinePath(int w, int h, int margin) {
    std::vector<Cell> out;
    if (w <= 0 || h <= 0) return out;
    const int top = margin;
    int bottom = h - 1 - margin;
    if (bottom < top) { bottom = top; }

    bool goingRight = true;
    for (int y = top; y <= bottom; y += 2) {
        if (goingRight) {
            for (int x = margin; x < w - margin; ++x) out.push_back(Cell{x, y});
        } else {
            for (int x = w - 1 - margin; x >= margin; --x) out.push_back(Cell{x, y});
        }
        if (y + 2 <= bottom) {
            // The connector column at the turn.
            const int cx = goingRight ? w - 1 - margin : margin;
            out.push_back(Cell{cx, y + 1});
        }
        goingRight = !goingRight;
    }
    return out;
}

// ------------------------------------------------------------------ pathfinding

int terrainStepCost(Terrain t) {
    switch (t) {
        // Path is much cheaper than grass, so enemies follow the intended route
        // when they have the choice. It is not forbidden, though: a tower on the
        // route makes them walk around, across grass, at a higher cost.
        case Terrain::Path: return 4;
        case Terrain::Sand: return 12;
        case Terrain::Grass: return 16;
        default: return 1000;             // impassable
    }
}

namespace {

struct OpenNode {
    int f = 0;
    int g = 0;
    Cell cell;
    bool operator<(const OpenNode& o) const { return f > o.f; }   // min-heap
};

int manhattan(Cell a, Cell b) {
    return std::abs(a.x - b.x) + std::abs(a.y - b.y);
}

}  // namespace

std::vector<Cell> astar(const Grid& grid, Cell from, Cell to,
                        const std::vector<int>& towers,
                        bool ignoreTowers) {
    std::vector<Cell> result;
    if (!grid.inBounds(from) || !grid.inBounds(to)) return result;

    const int w = grid.width(), h = grid.height();
    const size_t n = static_cast<size_t>(w) * h;

    auto idx = [w](Cell c) { return static_cast<size_t>(c.y) * w + c.x; };

    // A tower occupies its cell. A tower is 1x1 for simplicity, but the tower
    // table stores ids, so multiple towers never share a cell.
    auto blockedByTower = [&](Cell c) {
        if (ignoreTowers) return false;
        return towers[idx(c)] != 0;
    };

    // The start may itself hold a tower (e.g. an enemy standing where a tower
    // was just built); treat the start as passable so the search can begin.
    // The goal likewise.
    auto passable = [&](Cell c) {
        if (!grid.inBounds(c)) return false;
        if (c == from || c == to) return true;
        if (blockedByTower(c)) return false;
        return terrainStepCost(grid.at(c)) < 1000;
    };

    std::vector<int> gScore(n, INT32_MAX);
    std::vector<int> cameFrom(n, -1);
    std::vector<char> closed(n, 0);

    std::priority_queue<OpenNode> open;
    gScore[idx(from)] = 0;
    open.push(OpenNode{manhattan(from, to), 0, from});

    static const int DX[4] = {1, -1, 0, 0};
    static const int DY[4] = {0, 0, 1, -1};

    while (!open.empty()) {
        const OpenNode cur = open.top();
        open.pop();
        if (closed[idx(cur.cell)]) continue;
        closed[idx(cur.cell)] = 1;

        if (cur.cell == to) {
            // Walk the chain back.
            for (int c = idx(to); c != -1; c = cameFrom[c]) {
                result.push_back(Cell{c % w, c / w});
                if (c == idx(from)) break;
            }
            std::reverse(result.begin(), result.end());
            return result;
        }

        for (int d = 0; d < 4; ++d) {
            const Cell nb{cur.cell.x + DX[d], cur.cell.y + DY[d]};
            if (!passable(nb)) continue;
            const int tentative = gScore[idx(cur.cell)] + terrainStepCost(grid.at(nb));
            if (tentative >= gScore[idx(nb)]) continue;
            cameFrom[idx(nb)] = static_cast<int>(idx(cur.cell));
            gScore[idx(nb)] = tentative;
            open.push(OpenNode{tentative + manhattan(nb, to), tentative, nb});
        }
    }
    return result;   // empty == unreachable
}

std::vector<Cell> Grid::findPath(Cell from, Cell to, bool ignoreTowers) const {
    return astar(*this, from, to, towers_, ignoreTowers);
}

bool Grid::canReach(Cell from, Cell to, bool ignoreTowers) const {
    // A path of length 1 means from == to and the enemy is already home.
    if (from == to) return true;
    return !findPath(from, to, ignoreTowers).empty();
}

}  // namespace tf
