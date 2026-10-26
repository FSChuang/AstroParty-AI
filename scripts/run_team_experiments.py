#!/usr/bin/env python3
"""Batch-runs the AstroPartyAI-headless executable for Team Resource Battle
matches and summarizes the results.

This script is a thin experiment harness, not part of the simulation: it
only invokes the already-built headless executable as a subprocess and
parses the JSON it prints. It uses nothing but the Python standard library
(no ML/data-science dependencies), matching this project's rule that
Python stays outside the deterministic C++ simulation runtime.

Example:
    python3 scripts/run_team_experiments.py \\
        --executable build/AstroPartyAI-headless \\
        --config configs/team_resource_battle.json \\
        --map configs/maps/team_arena.json \\
        --team0-strategy strategic --team1-strategy baseline \\
        --runs 50 --csv results.csv
"""
import argparse
import csv
import json
import subprocess
import sys
from collections import Counter


def run_once(executable, config, map_path, max_ticks, team0_strategy, team1_strategy):
    command = [
        executable,
        "--config", config,
        "--map", map_path,
        "--max-ticks", str(max_ticks),
        "--team0-strategy", team0_strategy,
        "--team1-strategy", team1_strategy,
    ]
    completed = subprocess.run(command, capture_output=True, text=True, check=True)
    return json.loads(completed.stdout)


def summarize(results):
    outcomes = Counter(result["outcome"] for result in results)
    win_counts = Counter(result.get("winningTeamId") for result in results if result["outcome"] == "TeamWon")
    run_count = len(results)

    average_scores = [0.0, 0.0]
    average_resources_collected = [0.0, 0.0]
    average_ships_destroyed = [0.0, 0.0]
    average_ticks = 0.0
    for result in results:
        for team in (0, 1):
            average_scores[team] += result["teamScores"][team]
            average_resources_collected[team] += result["resourcesCollected"][team]
            average_ships_destroyed[team] += result["shipsDestroyed"][team]
        average_ticks += result["ticksElapsed"]
    if run_count > 0:
        average_scores = [value / run_count for value in average_scores]
        average_resources_collected = [value / run_count for value in average_resources_collected]
        average_ships_destroyed = [value / run_count for value in average_ships_destroyed]
        average_ticks /= run_count

    return {
        "runCount": run_count,
        "outcomeCounts": dict(outcomes),
        "team0WinRate": win_counts.get(0, 0) / run_count if run_count else 0.0,
        "team1WinRate": win_counts.get(1, 0) / run_count if run_count else 0.0,
        "drawRate": outcomes.get("Draw", 0) / run_count if run_count else 0.0,
        "averageTeamScores": average_scores,
        "averageResourcesCollected": average_resources_collected,
        "averageShipsDestroyed": average_ships_destroyed,
        "averageTicksElapsed": average_ticks,
    }


def write_csv(path, results):
    fieldnames = [
        "run", "outcome", "winningTeamId", "ticksElapsed",
        "team0Score", "team1Score",
        "team0ResourcesCollected", "team1ResourcesCollected",
        "team0ShipsDestroyed", "team1ShipsDestroyed",
    ]
    with open(path, "w", newline="") as csv_file:
        writer = csv.DictWriter(csv_file, fieldnames=fieldnames)
        writer.writeheader()
        for index, result in enumerate(results):
            writer.writerow({
                "run": index,
                "outcome": result["outcome"],
                "winningTeamId": result.get("winningTeamId"),
                "ticksElapsed": result["ticksElapsed"],
                "team0Score": result["teamScores"][0],
                "team1Score": result["teamScores"][1],
                "team0ResourcesCollected": result["resourcesCollected"][0],
                "team1ResourcesCollected": result["resourcesCollected"][1],
                "team0ShipsDestroyed": result["shipsDestroyed"][0],
                "team1ShipsDestroyed": result["shipsDestroyed"][1],
            })


def parse_args():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--executable", default="build/AstroPartyAI-headless", help="Path to AstroPartyAI-headless")
    parser.add_argument("--config", default="configs/team_resource_battle.json")
    parser.add_argument("--map", dest="map_path", default="configs/maps/team_arena.json")
    parser.add_argument("--max-ticks", type=int, default=3600)
    parser.add_argument("--team0-strategy", choices=["strategic", "baseline"], default="strategic")
    parser.add_argument("--team1-strategy", choices=["strategic", "baseline"], default="baseline")
    parser.add_argument("--runs", type=int, default=20)
    parser.add_argument("--csv", default=None, help="Optional path to write a per-run CSV of results")
    return parser.parse_args()


def main():
    args = parse_args()

    results = []
    for run_index in range(args.runs):
        try:
            result = run_once(args.executable, args.config, args.map_path, args.max_ticks,
                               args.team0_strategy, args.team1_strategy)
        except subprocess.CalledProcessError as error:
            print(f"Run {run_index} failed: {error.stderr}", file=sys.stderr)
            return 1
        results.append(result)

    summary = summarize(results)
    print(json.dumps(summary, indent=2))

    if args.csv:
        write_csv(args.csv, results)
        print(f"Wrote {len(results)} per-run rows to {args.csv}", file=sys.stderr)

    return 0


if __name__ == "__main__":
    sys.exit(main())
