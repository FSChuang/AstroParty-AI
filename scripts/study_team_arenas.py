#!/usr/bin/env python3
"""Run the procedural arena study and regenerate its distribution plots."""

import argparse
from collections import Counter
from concurrent.futures import ProcessPoolExecutor
from functools import partial
import json
import math
from pathlib import Path
import subprocess
from tempfile import TemporaryDirectory

from generate_team_arena import TEAM_CONFIG, InvalidArena, generate, validate, write_arena


FAMILIES = [(4, "central"), (4, "spread"), (8, "central"), (8, "spread")]
PRIMARY_SEEDS = 50
MAX_SEEDS = 200
SAMPLE = 30
RESOURCE_COUNT = 5
METRICS = [
    ("obstacle_coverage_pct", "Obstacle coverage (%)", 2),
    ("resource_midline_distance", "Mean normalized resource distance", 3),
    ("delivery_detour_ratio", "Mean delivery detour ratio", 3),
]


def run_matches(executable, map_path, max_ticks):
    matches = []
    for team0, team1 in (("strategic", "baseline"), ("baseline", "strategic")):
        command = [str(executable), "--config", str(TEAM_CONFIG), "--map", str(map_path),
                   "--max-ticks", str(max_ticks), "--team0-strategy", team0, "--team1-strategy", team1]
        try:
            result = subprocess.run(command, capture_output=True, text=True, timeout=15, check=True)
            outcome = json.loads(result.stdout)
            if (outcome["outcome"] not in ("TeamWon", "Draw")
                    or len(outcome["teamScores"]) != 2):
                raise ValueError("match did not reach a terminal team result")
            matches.append({"strategies": [team0, team1], "result": outcome})
        except (subprocess.SubprocessError, OSError, ValueError, KeyError) as exc:
            detail = exc.stderr if isinstance(exc, subprocess.CalledProcessError) else str(exc)
            matches.append({"strategies": [team0, team1], "error": detail.strip()})
    if any("error" in match for match in matches):
        return matches, "headless match failed"
    if any(not any(match["result"]["teamScores"][side] > 0 for match in matches)
           for side in (0, 1)):
        return matches, "at least one side did not deliver"
    return matches, None


def evaluate(seed, obstacles, layout, ship_radius, max_ticks, executable, temp):
    row = {"seed": seed, "obstacles": obstacles, "resources": RESOURCE_COUNT,
           "resource_layout": layout, "geometry_valid": False, "playable": False}
    try:
        arena = generate(seed, obstacles, RESOURCE_COUNT, layout, ship_radius)
    except InvalidArena as exc:
        return {**row, "rejection": f"placement: {exc}"}, None
    try:
        row["metrics"] = validate(arena, obstacles, RESOURCE_COUNT, ship_radius)
    except InvalidArena as exc:
        return {**row, "rejection": f"geometry: {exc}"}, None
    row["geometry_valid"] = True
    map_path = temp / (arena["name"] + ".json")
    write_arena(arena, map_path)
    row["matches"], rejection = run_matches(executable, map_path, max_ticks)
    row.update(playable=rejection is None, rejection=rejection)
    return row, arena


def summarize(rows):
    raw = len(rows)
    geometry_valid = sum(r["geometry_valid"] for r in rows)
    playable = sum(r["playable"] for r in rows)
    return {"raw": raw, "geometry_valid": geometry_valid, "playable": playable,
            "geometry_pass_fraction": geometry_valid / raw if raw else None,
            "playable_fraction": playable / raw if raw else None,
            "rejections": dict(Counter(r["rejection"] for r in rows if r["rejection"]))}


def layout_key(arena):
    rocks = sorted((rock["position"]["x"], rock["position"]["y"], rock["radius"])
                   for rock in arena["obstacles"])
    cores = sorted((core["x"], core["y"]) for core in arena["resourceSpawnPoints"])
    return tuple(rocks), tuple(cores)


def run_study(output, executable, jobs):
    if not executable.is_file():
        raise ValueError(f"build the headless executable first: {executable}")
    config = json.loads(TEAM_CONFIG.read_text())
    attempts = []
    summaries = []
    with TemporaryDirectory(prefix="astroparty-study-") as temp, ProcessPoolExecutor(jobs) as pool:
        for obstacles, layout in FAMILIES:
            analyze = partial(evaluate, obstacles=obstacles, layout=layout,
                              ship_radius=config["shipCollisionRadius"],
                              max_ticks=config["maxMatchTicks"], executable=executable, temp=Path(temp))
            rows = []
            seen = set()

            def record(row, arena):
                rows.append(row)
                if row["playable"] and len(seen) < SAMPLE:
                    key = layout_key(arena)
                    if key not in seen:
                        seen.add(key)
                        path = Path("maps") / (arena["name"] + ".json")
                        write_arena(arena, output / path)
                        row["sample_map"] = path.as_posix()

            for row, arena in pool.map(analyze, range(PRIMARY_SEEDS)):
                record(row, arena)
            for seed in range(PRIMARY_SEEDS, MAX_SEEDS):
                if len(seen) == SAMPLE:
                    break
                record(*analyze(seed))
            attempts.extend(rows)
            summaries.append({"obstacles": obstacles, "resource_layout": layout,
                              "primary": summarize(rows[:PRIMARY_SEEDS]),
                              "supplemental": summarize(rows[PRIMARY_SEEDS:]),
                              "sample_size": len(seen)})
            print(f"{obstacles} obstacles, {layout}: {summaries[-1]}", flush=True)
    report = {
        "team_config": config, "families": summaries,
        "primary": summarize([r for r in attempts if r["seed"] < PRIMARY_SEEDS]),
        "attempts": attempts,
    }
    (output / "study.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    write_plots(report, output)
    return all(row["sample_size"] == SAMPLE for row in summaries)


def write_plots(report, output):
    groups = [[row["metrics"] for row in report["attempts"]
               if row.get("sample_map") and (row["obstacles"], row["resource_layout"]) == family]
              for family in FAMILIES]
    if not any(groups):
        return
    # Common bins and y scale within each metric make the four families comparable.
    for key, title, precision in METRICS:
        values = [item[key] for group in groups for item in group]
        low, high = min(values), max(values)
        if math.isclose(low, high):
            low, high = low - 0.01, high + 0.01
        bins = 12
        histograms = [Counter(min(bins - 1, int((item[key] - low) * bins / (high - low)))
                              for item in group) for group in groups]
        peak = max(count for counts in histograms for count in counts.values())
        top = max(4, 4 * math.ceil(peak / 4))
        svg = ['<svg xmlns="http://www.w3.org/2000/svg" width="920" height="340" viewBox="0 0 920 340" '
               f'role="img" aria-label="Histograms of {title} by arena family">',
               '<rect width="920" height="340" fill="white"/>',
               '<g font-family="Arial, sans-serif" fill="#162433">',
               f'<text x="460" y="31" text-anchor="middle" font-weight="bold" font-size="22">{title}</text>',
               '<text transform="translate(22 175) rotate(-90)" text-anchor="middle" font-size="14">Maps per bin</text>']
        for i, ((obstacles, layout), counts, group) in enumerate(zip(FAMILIES, histograms, groups)):
            x0, y0, width, height = 66 + 214 * i, 255, 178, 157
            svg.append(f'<text x="{x0 + width / 2}" y="65" text-anchor="middle" font-size="15" '
                       f'font-weight="bold">{obstacles} rocks · {layout}</text>')
            svg.append(f'<text x="{x0 + width / 2}" y="84" text-anchor="middle" font-size="12">n = {len(group)}</text>')
            for tick in range(5):
                y = y0 - height * tick / 4
                svg.append(f'<path d="M {x0} {y} h {width}" stroke="#d7e0e7"/>')
                if i == 0:
                    svg.append(f'<text x="{x0 - 6}" y="{y + 4}" text-anchor="end" font-size="12">{tick * top // 4}</text>')
            color = "#236a8a" if layout == "central" else "#9b531a"
            for j, count in sorted(counts.items()):
                bar_width = width / bins
                bar_height = height * count / top
                svg.append(f'<rect x="{x0 + j * bar_width + 0.5}" y="{y0 - bar_height}" '
                           f'width="{bar_width - 1}" height="{bar_height}" fill="{color}"/>')
            for fraction in (0, 0.5, 1):
                x = x0 + width * fraction
                label = low + (high - low) * fraction
                svg.append(f'<text x="{x}" y="{y0 + 22}" text-anchor="middle" font-size="12">{label:.{precision}f}</text>')
        svg.append('<text x="460" y="317" text-anchor="middle" font-size="13" fill="#425466">'
                   'Accepted maps · 12 shared bins per metric</text></g></svg>')
        (output / f"{key}.svg").write_text("\n".join(svg) + "\n", encoding="utf-8")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=Path("out/procedural-arenas"))
    parser.add_argument("--executable", type=Path, default=Path("build/AstroPartyAI-headless"))
    parser.add_argument("--jobs", type=int, default=4)
    parser.add_argument("--plots-only", action="store_true", help="replot the study.json in the output directory")
    args = parser.parse_args()
    if args.jobs < 1:
        parser.error("--jobs must be positive")
    args.output.mkdir(parents=True, exist_ok=True)
    try:
        if args.plots_only:
            write_plots(json.loads((args.output / "study.json").read_text()), args.output)
        elif not run_study(args.output, args.executable.resolve(), args.jobs):
            parser.exit(1, "Study shortfall: at least one family did not reach 30 playable maps. See study.json.\n")
    except (OSError, ValueError) as exc:
        parser.error(str(exc))


if __name__ == "__main__":
    main()
