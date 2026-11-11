#!/usr/bin/env python3
"""Generate and validate a seeded AstroParty-AI Team Resource Battle arena."""

import argparse
import heapq
import itertools
import json
import math
import random
from pathlib import Path
from typing import NamedTuple, Optional, Tuple


class ArenaPreset(NamedTuple):
    dimensions: Tuple[int, int]
    winning_score: int
    duration_seconds: Optional[int]
    obstacles: int


WIDTH, HEIGHT = 1280, 720
ARENA_PRESETS = {
    "small": ArenaPreset((960, 540), 3, None, 2),
    "medium": ArenaPreset((WIDTH, HEIGHT), 5, None, 4),
    "large": ArenaPreset((1600, 900), 7, 90, 6),
}
GRID_STEP = 20
CLEARANCE_MARGIN = 4
PAIR_TRIES = 100
RANDOM_SEED_TRIES = 20
TEAM_CONFIG = Path(__file__).resolve().parents[1] / "configs/team_resource_battle.json"


class InvalidArena(ValueError):
    """An arena candidate failed placement or validation."""


def point(position):
    return position["x"], position["y"]


def position(xy):
    return dict(zip(("x", "y"), xy))


def opposite(xy, arena_size=(WIDTH, HEIGHT)):
    return arena_size[0] - xy[0], arena_size[1] - xy[1]


def check_parameters(obstacles, resources, layout):
    if obstacles not in range(2, 13, 2):
        raise ValueError("obstacle count must be even and between 2 and 12")
    if resources not in range(3, 10):
        raise ValueError("resource count must be between 3 and 9")
    if layout not in ("central", "spread"):
        raise ValueError("resource layout must be central or spread")


def sample_pairs(rng, arena_size, count, x_range, y_range, radius_range, occupied, gap, label):
    circles = []
    for _ in range(count):
        for _ in range(PAIR_TRIES):
            xy = (rng.randint(*x_range), rng.randint(*y_range))
            radius = rng.randint(*radius_range)
            other = opposite(xy, arena_size)
            if math.dist(xy, other) <= 2 * radius + gap:
                continue
            if any(math.dist(center, fixed) <= radius + size + gap
                   for center in (xy, other) for fixed, size in occupied + circles):
                continue
            circles.extend([(xy, radius), (other, radius)])
            break
        else:
            raise InvalidArena(f"could not place {label} pair")
    return circles


def generate(seed, obstacles=None, resources=5, layout="spread", ship_radius=18, size="medium"):
    if size not in ARENA_PRESETS:
        raise ValueError(f"unknown arena size: {size}")
    preset = ARENA_PRESETS[size]
    obstacles = preset.obstacles if obstacles is None else obstacles
    check_parameters(obstacles, resources, layout)
    arena_size = preset.dimensions
    width, height = arena_size
    scale = width / WIDTH

    def scaled(value):
        return round(value * scale)

    def scaled_range(values):
        return tuple(scaled(value) for value in values)

    rng = random.Random(seed)
    left_home = (scaled(100), height // 2)
    left_spawns = [(left_home[0], scaled(y)) for y in (200, 360, 520)]
    homes = [left_home, opposite(left_home, arena_size)]
    home_radius = scaled(80)
    spawn_groups = [left_spawns, [opposite(xy, arena_size) for xy in left_spawns]]
    centers = [((width // 2, height // 2), 0)] if resources % 2 else []
    resource_band = (490, 575) if layout == "central" else (330, 460)
    cores = centers + sample_pairs(
        rng, arena_size, resources // 2, scaled_range(resource_band), scaled_range((110, 610)),
        (0, 0), centers, scaled(64), "resource"
    )
    clearance = ship_radius + CLEARANCE_MARGIN
    reserved = ([(xy, home_radius) for xy in homes]
                + [(xy, clearance) for group in spawn_groups for xy in group]
                + [(xy, clearance) for xy, _ in cores])
    rocks = sample_pairs(
        rng, arena_size, obstacles // 2, scaled_range((280, 620)), scaled_range((80, 640)),
        scaled_range((30, 50)), reserved, scaled(12), "obstacle"
    )
    prefix = "arena" if size == "medium" else f"arena-{size}"
    arena = {
        "name": f"{prefix}-{obstacles}-{resources}-{layout}-{seed}",
        "arenaSize": position(arena_size),
        "winningScore": preset.winning_score,
        "shipSpawns": [
            {"position": position(xy), "headingDegrees": 90 + 180 * team, "teamId": team}
            for team, group in enumerate(spawn_groups) for xy in group
        ],
        "homeRegions": [
            {"teamId": team, "position": position(xy), "radius": home_radius}
            for team, xy in enumerate(homes)
        ],
        "obstacles": [{"position": position(xy), "radius": radius} for xy, radius in rocks],
        "resourceSpawnPoints": [position(xy) for xy, _ in cores],
    }
    if preset.duration_seconds is not None:
        arena["matchDurationSeconds"] = preset.duration_seconds
    return arena


def segment_clear(start, end, circles):
    x, y = start
    dx, dy = end[0] - x, end[1] - y
    length_sq = dx * dx + dy * dy
    for (cx, cy), radius in circles:
        t = ((cx - x) * dx + (cy - y) * dy) / length_sq if length_sq else 0
        t = min(1, max(0, t))
        if (cx - x - t * dx) ** 2 + (cy - y - t * dy) ** 2 <= radius ** 2:
            return False
    return True


def inside(xy, margin, arena_size):
    x, y = xy
    width, height = arena_size
    return margin <= x <= width - margin and margin <= y <= height - margin


def navigation_graph(anchors, circles, clearance, arena_size):
    nodes = {}
    step = GRID_STEP
    width, height = arena_size
    for x in range(step, width, step):
        for y in range(step, height, step):
            xy = (x, y)
            if inside(xy, clearance, arena_size) and segment_clear(xy, xy, circles):
                nodes[xy] = []

    def connect(a, b):
        if a != b and segment_clear(a, b, circles):
            cost = math.dist(a, b)
            nodes[a].append((b, cost))
            nodes[b].append((a, cost))

    for x, y in nodes:
        for dx, dy in ((step, 0), (step, step), (0, step), (-step, step)):
            neighbor = (x + dx, y + dy)
            if neighbor in nodes:
                connect((x, y), neighbor)

    for anchor in dict.fromkeys(anchors):
        if anchor in nodes:
            continue
        nodes[anchor] = []
        gx, gy = (int(value // step) * step for value in anchor)
        for x in range(gx - step, gx + 2 * step + 1, step):
            for y in range(gy - step, gy + 2 * step + 1, step):
                if (x, y) in nodes:
                    connect(anchor, (x, y))

    # Exact visible anchor-to-anchor routes avoid grid artifacts on open lanes.
    for a, b in itertools.combinations(dict.fromkeys(anchors), 2):
        connect(a, b)
    return nodes


def route_distances(graph, start):
    distances = {start: 0.0}
    pending = [(0.0, start)]
    while pending:
        distance, node = heapq.heappop(pending)
        if distance != distances[node]:
            continue
        for neighbor, cost in graph[node]:
            candidate = distance + cost
            if candidate < distances.get(neighbor, math.inf):
                distances[neighbor] = candidate
                heapq.heappush(pending, (candidate, neighbor))
    return distances


def validate(arena, obstacles, resources, ship_radius):
    arena_size = point(arena["arenaSize"])
    for preset in ARENA_PRESETS.values():
        if arena_size == preset.dimensions:
            arena_size = preset.dimensions
            break
    else:
        raise InvalidArena("unknown arena dimensions")
    if arena.get("winningScore", preset.winning_score) != preset.winning_score:
        raise InvalidArena("winning score does not match arena dimensions")
    if arena.get("matchDurationSeconds") != preset.duration_seconds:
        raise InvalidArena("match duration does not match arena dimensions")
    width, height = arena_size
    spawns = arena["shipSpawns"]
    homes = arena["homeRegions"]
    cores = [point(xy) for xy in arena["resourceSpawnPoints"]]
    rocks = [(point(rock["position"]), rock["radius"]) for rock in arena["obstacles"]]
    if sorted(spawn["teamId"] for spawn in spawns) != [0, 0, 0, 1, 1, 1]:
        raise InvalidArena("expected three ship spawns per team")
    if sorted(home["teamId"] for home in homes) != [0, 1]:
        raise InvalidArena("expected one home region per team")
    if len(rocks) != obstacles or len(cores) != resources or len(set(cores)) != resources:
        raise InvalidArena("unexpected obstacle or resource count")
    bases = [point(home["position"]) for home in homes]
    clearance = ship_radius + CLEARANCE_MARGIN
    if any(not 0 < h["radius"] or not inside(xy, h["radius"], arena_size) for h, xy in zip(homes, bases)):
        raise InvalidArena("home region lies outside arena")
    if math.dist(*bases) <= sum(home["radius"] for home in homes):
        raise InvalidArena("overlapping home regions")
    if any(not 0 < radius or not inside(xy, radius, arena_size) for xy, radius in rocks):
        raise InvalidArena("obstacle lies outside arena or has invalid radius")
    if any(math.dist(a, b) <= ra + rb for (a, ra), (b, rb) in itertools.combinations(rocks, 2)):
        raise InvalidArena("overlapping obstacles")
    if any(math.dist(xy, base) <= radius + home["radius"]
           for xy, radius in rocks for base, home in zip(bases, homes)):
        raise InvalidArena("obstacle intrudes into home region")
    expanded = [(xy, radius + clearance) for xy, radius in rocks]
    spawn_points = [point(spawn["position"]) for spawn in spawns]
    if any(math.dist(a, b) < 2 * ship_radius for a, b in itertools.combinations(spawn_points, 2)):
        raise InvalidArena("overlapping ship spawns")
    if any(core in bases for core in cores):
        raise InvalidArena("resource overlaps home center")
    for label, points in (("ship spawn", spawn_points), ("home center", bases), ("resource", cores)):
        if any(not inside(xy, clearance, arena_size) or not segment_clear(xy, xy, expanded) for xy in points):
            raise InvalidArena(f"blocked {label}")
    anchors = bases + spawn_points + cores
    graph = navigation_graph(anchors, expanded, clearance, arena_size)
    distances = route_distances(graph, bases[0])
    if any(xy not in distances for xy in anchors):
        raise InvalidArena("disconnected home, spawn, or resource")
    routes = [distances, route_distances(graph, bases[1])]
    return {
        "obstacle_coverage_pct": 100 * sum(math.pi * radius ** 2 for _, radius in rocks) / (width * height),
        "resource_midline_distance": sum(abs(x - width / 2) / (width / 2) for x, _ in cores) / len(cores),
        "delivery_detour_ratio": sum(route[xy] / math.dist(base, xy)
                                     for base, route in zip(bases, routes) for xy in cores) / (2 * len(cores)),
    }


def write_arena(arena, path):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(arena, indent=2) + "\n", encoding="utf-8")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--seed", type=int, help="replay a layout (defaults to a random seed)")
    parser.add_argument("--size", choices=ARENA_PRESETS, default="medium")
    parser.add_argument("--obstacles", type=int, help="override the preset with an even count from 2 to 12")
    parser.add_argument("--resources", type=int, default=5)
    parser.add_argument("--resource-layout", choices=("central", "spread"), default="spread")
    parser.add_argument("--ship-radius", type=float, help="use the active game's ship radius for clearance")
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    obstacles = ARENA_PRESETS[args.size].obstacles if args.obstacles is None else args.obstacles
    try:
        check_parameters(obstacles, args.resources, args.resource_layout)
        ship_radius = args.ship_radius
        if ship_radius is None:
            ship_radius = json.loads(TEAM_CONFIG.read_text())["shipCollisionRadius"]
        if not math.isfinite(ship_radius) or ship_radius <= 0:
            raise ValueError("ship radius must be positive and finite")
        rng = random.SystemRandom()
        seeds = (args.seed,) if args.seed is not None else (rng.getrandbits(64) for _ in range(RANDOM_SEED_TRIES))
        for seed in seeds:
            try:
                arena = generate(seed, obstacles, args.resources, args.resource_layout, ship_radius, args.size)
                validate(arena, obstacles, args.resources, ship_radius)
            except InvalidArena as exc:
                if args.seed is not None:
                    raise InvalidArena(f"seed {seed}: {exc}") from exc
                continue
            write_arena(arena, args.output)
            print(f"Generated {args.size} arena with seed {seed} (first to {arena['winningScore']})", flush=True)
            return
        raise InvalidArena(f"could not generate a valid arena in {RANDOM_SEED_TRIES} attempts")
    except (OSError, ValueError) as exc:
        parser.error(str(exc))


if __name__ == "__main__":
    main()
