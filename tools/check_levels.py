"""
Validates the level maps and routes in src/core/Levels.cpp.

Hand-drawing a route on a character map is error-prone — a waypoint two tiles
off lands inside a rock. This script parses the maps straight out of the C++
source, checks every route segment against the terrain, and prints an annotated
map so a fix is obvious.

Usage:  python tools/check_levels.py
"""

import re
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
LEVELS_CPP = HERE.parent / "src" / "core" / "Levels.cpp"

BLOCKING = set("#~")


def parse_levels():
    """Pulls the level blocks out of Levels.cpp."""
    text = LEVELS_CPP.read_text(encoding="utf-8")
    levels = []

    # Each level is a block with rows = { "..." ... } and waypoints = { Cell{x, y}, ... }
    for match in re.finditer(
        r'l\.rows\s*=\s*\{(.*?)\};.*?l\.waypoints\s*=\s*\{(.*?)\};',
        text, re.S):
        rows_block, way_block = match.group(1), match.group(2)
        rows = re.findall(r'"([^"]*)"', rows_block)
        cells = [(int(a), int(b)) for a, b in
                 re.findall(r'Cell\{(\d+),\s*(\d+)\}', way_block)]
        levels.append({"rows": rows, "waypoints": cells})

    return levels


def route_cells(waypoints):
    """Every cell the straight segments between waypoints pass through."""
    out = []
    for i in range(len(waypoints) - 1):
        ax, ay = waypoints[i]
        bx, by = waypoints[i + 1]
        dx, dy = bx - ax, by - ay
        steps = max(abs(dx), abs(dy))
        if steps == 0:
            out.append((ax, ay))
            continue
        for s in range(steps + 1):
            t = s / steps
            x = ax + round(dx * t)
            y = ay + round(dy * t)
            out.append((x, y))
    return out


def check(level, index):
    rows = level["rows"]
    waypoints = level["waypoints"]
    problems = []

    width = len(rows[0]) if rows else 0
    for i, r in enumerate(rows):
        if len(r) != width:
            problems.append(f"row {i} has width {len(r)}, expected {width}")

    # Start and exit must exist exactly once.
    starts = [(x, y) for y, r in enumerate(rows) for x, c in enumerate(r) if c == "S"]
    ends = [(x, y) for y, r in enumerate(rows) for x, c in enumerate(r) if c == "E"]
    if len(starts) != 1:
        problems.append(f"expected exactly one 'S', found {len(starts)}")
    if len(ends) != 1:
        problems.append(f"expected exactly one 'E', found {len(ends)}")

    for (x, y) in waypoints:
        if not (0 <= y < len(rows) and 0 <= x < width):
            problems.append(f"waypoint ({x},{y}) is outside the map")
            continue
        if x >= len(rows[y]):
            problems.append(f"waypoint ({x},{y}) is past the end of row {y} "
                            f"(length {len(rows[y])}, expected {width})")
            continue
        if rows[y][x] in BLOCKING:
            problems.append(f"waypoint ({x},{y}) is inside a wall ('{rows[y][x]}')")

    for (x, y) in route_cells(waypoints):
        if not (0 <= y < len(rows) and 0 <= x < width):
            problems.append(f"route passes outside the map at ({x},{y})")
            continue
        if x >= len(rows[y]):
            problems.append(f"route passes through a short row at ({x},{y})")
            continue
        if rows[y][x] in BLOCKING:
            problems.append(
                f"route passes through a wall at ({x},{y}) ('{rows[y][x]}')")

    # The route must connect the entry to the exit.
    if starts and ends:
        if waypoints[0] != starts[0]:
            problems.append(f"route starts at {waypoints[0]}, not at the entry {starts[0]}")
        if waypoints[-1] != ends[0]:
            problems.append(f"route ends at {waypoints[-1]}, not at the exit {ends[0]}")

    return problems


def print_map(level):
    """An annotated map: '#' wall, '.' grass, '~' water, and the route marked."""
    rows = level["rows"]
    route = set(route_cells(level["waypoints"]))
    print()
    print("    " + "".join(str(x % 10) for x in range(len(rows[0]))))
    for y, r in enumerate(rows):
        line = ""
        for x, c in enumerate(r):
            if (x, y) in route and c not in BLOCKING:
                line += "=" if c not in "SE" else c
            else:
                line += c
        print(f"{y:3} {line}")


def main():
    levels = parse_levels()
    if not levels:
        print("❌ no levels found in Levels.cpp")
        return 1

    failed = 0
    for i, level in enumerate(levels):
        problems = check(level, i)
        name = f"level {i + 1}"
        if problems:
            failed += 1
            print(f"❌ {name}: {len(problems)} problem(s)")
            for p in problems[:12]:
                print(f"     - {p}")
            if len(problems) > 12:
                print(f"     ... and {len(problems) - 12} more")
        else:
            print(f"✅ {name}: route is valid ({len(route_cells(level['waypoints']))} tiles)")
        print_map(level)

    if failed:
        print(f"\n❌ {failed} level(s) have problems")
        return 1
    print("\n✅ all levels valid")
    return 0


if __name__ == "__main__":
    sys.exit(main())
