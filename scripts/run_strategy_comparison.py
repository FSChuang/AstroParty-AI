#!/usr/bin/env python3
"""Milestone 5 strategy comparison: how do different role-assignment
algorithms perform across different map geometries?

This script is an orchestration + analysis harness around the actual C++
simulation -- it never reimplements gameplay or AI logic. It:

  1. Generates N maps per family (open/clustered/chokepoint) with
     AstroPartyAI-mapgen (subprocess), recording each map's seed and
     geometry metrics (from mapgen's own manifest.csv).
  2. For every (map, strategy pair, side assignment) combination, runs one
     deterministic headless match with AstroPartyAI-headless --diagnostics
     (subprocess), parsing the combined result+diagnostics JSON.
  3. Writes the full per-match table to experiments/results/matches.csv.
  4. Computes win rates, resource-delivery performance, timeout/low-
     progress rates, and a MAP-PAIRED bootstrap comparison between each
     pair of strategies (resampling over MAPS, the actual independent
     unit -- the two side-swapped matches per map are a paired
     observation, not two independent samples), writing
     experiments/analysis/summary.csv, pairwise_comparison.csv, and a
     handful of plots.

Read configuration from experiments/configs/strategy_comparison.json by
default; every value can be overridden on the command line.

Reproduction:
    python3 scripts/run_strategy_comparison.py \\
        --mapgen-executable build/AstroPartyAI-mapgen \\
        --headless-executable build/AstroPartyAI-headless
"""
import argparse
import csv
import json
import os
import random
import statistics
import subprocess
import sys
from itertools import combinations

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

DEFAULT_CONFIG_PATH = "experiments/configs/strategy_comparison.json"
BOOTSTRAP_RESAMPLES = 2000


def load_experiment_config(path):
    with open(path) as f:
        return json.load(f)


def generate_maps(mapgen_executable, generator_configs_dir, maps_dir, families, maps_per_family, seed_start):
    """Returns {family: [ {seed, filename, mapPath, <geometry metrics>...} ]}."""
    maps_by_family = {}
    for family in families:
        config_path = os.path.join(generator_configs_dir, f"{family}.json")
        output_dir = os.path.join(maps_dir, family)
        command = [
            mapgen_executable, "--config", config_path, "--seed", str(seed_start),
            "--count", str(maps_per_family), "--output-dir", output_dir, "--label", family,
        ]
        subprocess.run(command, capture_output=True, text=True, check=True)

        rows = []
        with open(os.path.join(output_dir, "manifest.csv"), newline="") as manifest_file:
            for row in csv.DictReader(manifest_file):
                row["family"] = family
                row["mapPath"] = os.path.join(output_dir, row["filename"])
                rows.append(row)
        maps_by_family[family] = rows
        print(f"Generated {len(rows)} '{family}' maps (seeds {seed_start}..{seed_start + maps_per_family - 1})",
              file=sys.stderr)
    return maps_by_family


def run_match(headless_executable, simulation_config_path, map_path, team0_strategy, team1_strategy, max_ticks):
    command = [
        headless_executable, "--config", simulation_config_path, "--map", map_path,
        "--max-ticks", str(max_ticks), "--team0-strategy", team0_strategy,
        "--team1-strategy", team1_strategy, "--diagnostics",
    ]
    completed = subprocess.run(command, capture_output=True, text=True, check=True)
    return json.loads(completed.stdout)


def summarize_team_diagnostics(team_diagnostics):
    ships = team_diagnostics["ships"]
    latencies = team_diagnostics["deliveryLatenciesTicks"]
    return {
        "shotsFired": sum(s["shotsFired"] for s in ships),
        "obstacleCollisionEvents": sum(s["obstacleCollisionEvents"] for s in ships),
        "lowProgressTicks": sum(s["lowProgressTicks"] for s in ships),
        "roleChanges": sum(s["roleChanges"] for s in ships),
        "deliveries": len(latencies),
        "avgDeliveryLatencyTicks": (sum(latencies) / len(latencies)) if latencies else "",
    }


def flatten_match_row(map_row, team0_strategy, team1_strategy, payload):
    result = payload["result"]
    diagnostics = payload["diagnostics"]
    row = {
        "family": map_row["family"],
        "mapFile": os.path.basename(map_row["mapPath"]),
        "seed": map_row["seed"],
        "obstacleCoverageRatio": map_row["obstacleCoverageRatio"],
        "obstacleClusterCount": map_row["obstacleClusterCount"],
        "teamAccessibilityImbalance": map_row["teamAccessibilityImbalance"],
        "team0Strategy": team0_strategy,
        "team1Strategy": team1_strategy,
        "outcome": result["outcome"],
        "winningTeamId": result.get("winningTeamId"),
        "ticksElapsed": result["ticksElapsed"],
        "team0Score": result["teamScores"][0],
        "team1Score": result["teamScores"][1],
        "team0ResourcesCollected": result["resourcesCollected"][0],
        "team1ResourcesCollected": result["resourcesCollected"][1],
        "team0ShipsDestroyed": result["shipsDestroyed"][0],
        "team1ShipsDestroyed": result["shipsDestroyed"][1],
    }
    for team_key, prefix in (("team0", "team0_"), ("team1", "team1_")):
        for metric, value in summarize_team_diagnostics(diagnostics[team_key]).items():
            row[prefix + metric] = value
    return row


def run_all_matches(headless_executable, simulation_config_path, maps_by_family, strategies, max_ticks):
    rows = []
    strategy_pairs = list(combinations(strategies, 2))
    for family, maps in maps_by_family.items():
        for map_row in maps:
            for strategy_a, strategy_b in strategy_pairs:
                for team0_strategy, team1_strategy in ((strategy_a, strategy_b), (strategy_b, strategy_a)):
                    payload = run_match(headless_executable, simulation_config_path, map_row["mapPath"],
                                        team0_strategy, team1_strategy, max_ticks)
                    rows.append(flatten_match_row(map_row, team0_strategy, team1_strategy, payload))
        print(f"Completed all matches for family '{family}'", file=sys.stderr)
    return rows


def write_matches_csv(path, rows):
    if not rows:
        return
    fieldnames = sorted({key for row in rows for key in row.keys()})
    with open(path, "w", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=fieldnames)
        writer.writeheader()
        writer.writerows(rows)


def strategy_outcome_for_row(row, strategy):
    """Returns (is_win, score, resources_collected, ships_destroyed, deliveries,
    avg_delivery_latency, low_progress_ticks, role_changes, shots_fired) for
    whichever side `strategy` played in this match row, or None if `strategy`
    did not play in it."""
    if row["team0Strategy"] == strategy:
        side = "team0"
    elif row["team1Strategy"] == strategy:
        side = "team1"
    else:
        return None
    winning_team_id = row["winningTeamId"]
    is_win = winning_team_id is not None and int(winning_team_id) == (0 if side == "team0" else 1)
    return {
        "win": is_win,
        "score": int(row[f"{side}Score"]),
        "resourcesCollected": int(row[f"{side}ResourcesCollected"]),
        "shipsDestroyed": int(row[f"{side}ShipsDestroyed"]),
        "deliveries": int(row[f"{side}_deliveries"]),
        "avgDeliveryLatencyTicks": float(row[f"{side}_avgDeliveryLatencyTicks"])
        if row[f"{side}_avgDeliveryLatencyTicks"] != "" else None,
        "lowProgressTicks": int(row[f"{side}_lowProgressTicks"]),
        "roleChanges": int(row[f"{side}_roleChanges"]),
        "shotsFired": int(row[f"{side}_shotsFired"]),
        "timeout": row["outcome"] == "Timeout",
    }


def write_strategy_family_summary(path, rows, strategies, families):
    with open(path, "w", newline="") as f:
        fieldnames = ["strategy", "family", "matches", "winRate", "avgScore", "avgResourcesCollected",
                      "avgDeliveries", "avgDeliveryLatencyTicks", "timeoutRate", "avgLowProgressTicks",
                      "avgRoleChanges", "avgShotsFired"]
        writer = csv.DictWriter(f, fieldnames=fieldnames)
        writer.writeheader()
        for strategy in strategies:
            for family in families:
                outcomes = [strategy_outcome_for_row(row, strategy) for row in rows if row["family"] == family]
                outcomes = [o for o in outcomes if o is not None]
                if not outcomes:
                    continue
                latencies = [o["avgDeliveryLatencyTicks"] for o in outcomes if o["avgDeliveryLatencyTicks"] is not None]
                writer.writerow({
                    "strategy": strategy,
                    "family": family,
                    "matches": len(outcomes),
                    "winRate": statistics.fmean(o["win"] for o in outcomes),
                    "avgScore": statistics.fmean(o["score"] for o in outcomes),
                    "avgResourcesCollected": statistics.fmean(o["resourcesCollected"] for o in outcomes),
                    "avgDeliveries": statistics.fmean(o["deliveries"] for o in outcomes),
                    "avgDeliveryLatencyTicks": statistics.fmean(latencies) if latencies else "",
                    "timeoutRate": statistics.fmean(o["timeout"] for o in outcomes),
                    "avgLowProgressTicks": statistics.fmean(o["lowProgressTicks"] for o in outcomes),
                    "avgRoleChanges": statistics.fmean(o["roleChanges"] for o in outcomes),
                    "avgShotsFired": statistics.fmean(o["shotsFired"] for o in outcomes),
                })


def bootstrap_mean_difference_ci(paired_differences, resamples=BOOTSTRAP_RESAMPLES, seed=42):
    """Percentile bootstrap CI for the mean of `paired_differences`, resampling
    MAPS (each entry here is already one map's paired outcome) with
    replacement -- never resampling individual matches, since the two side-
    swapped matches per map are not independent observations."""
    if not paired_differences:
        return (0.0, 0.0, 0.0)
    rng = random.Random(seed)
    n = len(paired_differences)
    means = []
    for _ in range(resamples):
        sample = [paired_differences[rng.randrange(n)] for _ in range(n)]
        means.append(statistics.fmean(sample))
    means.sort()
    lower = means[int(0.025 * resamples)]
    upper = means[int(0.975 * resamples) - 1]
    return (statistics.fmean(paired_differences), lower, upper)


def write_pairwise_comparison(path, rows, strategies, families):
    """For each (strategyA, strategyB, family), pairs up the two matches
    played on the SAME map (one with A as team0, one as team1) into one
    per-map observation: strategyA's win rate across both its sides on that
    map minus strategyB's. Bootstraps a 95% CI over maps."""
    with open(path, "w", newline="") as f:
        fieldnames = ["strategyA", "strategyB", "family", "maps", "meanWinRateDiffAminusB", "ci95Low", "ci95High"]
        writer = csv.DictWriter(f, fieldnames=fieldnames)
        writer.writeheader()

        for strategy_a, strategy_b in combinations(strategies, 2):
            for family in families:
                by_map = {}
                for row in rows:
                    if row["family"] != family:
                        continue
                    pair = {row["team0Strategy"], row["team1Strategy"]}
                    if pair != {strategy_a, strategy_b}:
                        continue
                    by_map.setdefault(row["mapFile"], []).append(row)

                paired_diffs = []
                for map_file, match_rows in by_map.items():
                    a_outcomes = [strategy_outcome_for_row(r, strategy_a) for r in match_rows]
                    b_outcomes = [strategy_outcome_for_row(r, strategy_b) for r in match_rows]
                    a_outcomes = [o for o in a_outcomes if o is not None]
                    b_outcomes = [o for o in b_outcomes if o is not None]
                    if not a_outcomes or not b_outcomes:
                        continue
                    a_win_rate = statistics.fmean(o["win"] for o in a_outcomes)
                    b_win_rate = statistics.fmean(o["win"] for o in b_outcomes)
                    paired_diffs.append(a_win_rate - b_win_rate)

                mean_diff, ci_low, ci_high = bootstrap_mean_difference_ci(paired_diffs)
                writer.writerow({
                    "strategyA": strategy_a, "strategyB": strategy_b, "family": family,
                    "maps": len(paired_diffs), "meanWinRateDiffAminusB": mean_diff,
                    "ci95Low": ci_low, "ci95High": ci_high,
                })


def plot_win_rate_by_family(summary_path, analysis_dir, strategies, families):
    data = {strategy: [] for strategy in strategies}
    with open(summary_path, newline="") as f:
        for row in csv.DictReader(f):
            data[row["strategy"]].append((row["family"], float(row["winRate"])))

    fig, ax = plt.subplots(figsize=(8, 4.5))
    width = 0.25
    x = range(len(families))
    for i, strategy in enumerate(strategies):
        rates = [next((rate for fam, rate in data[strategy] if fam == family), 0.0) for family in families]
        ax.bar([pos + i * width for pos in x], rates, width, label=strategy)
    ax.set_xticks([pos + width for pos in x])
    ax.set_xticklabels(families)
    ax.set_ylabel("win rate")
    ax.set_title("Win rate by strategy and map family")
    ax.legend()
    fig.tight_layout()
    fig.savefig(os.path.join(analysis_dir, "win_rate_by_family.png"), dpi=150)
    plt.close(fig)


def plot_delivery_latency_by_strategy(matches, analysis_dir, strategies):
    data = []
    for strategy in strategies:
        latencies = []
        for row in matches:
            outcome = strategy_outcome_for_row(row, strategy)
            if outcome and outcome["avgDeliveryLatencyTicks"] is not None:
                latencies.append(outcome["avgDeliveryLatencyTicks"])
        data.append(latencies)

    fig, ax = plt.subplots(figsize=(7, 4))
    ax.boxplot(data, tick_labels=strategies)
    ax.set_ylabel("average delivery latency (ticks)")
    ax.set_title("Resource delivery latency by strategy")
    fig.tight_layout()
    fig.savefig(os.path.join(analysis_dir, "delivery_latency_by_strategy.png"), dpi=150)
    plt.close(fig)


def plot_low_progress_by_strategy(matches, analysis_dir, strategies):
    data = []
    for strategy in strategies:
        fractions = []
        for row in matches:
            outcome = strategy_outcome_for_row(row, strategy)
            if outcome and int(row["ticksElapsed"]) > 0:
                fractions.append(outcome["lowProgressTicks"] / (3 * int(row["ticksElapsed"])))
        data.append(fractions)

    fig, ax = plt.subplots(figsize=(7, 4))
    ax.boxplot(data, tick_labels=strategies)
    ax.set_ylabel("fraction of ship-ticks flagged low-progress")
    ax.set_title("Low-progress rate by strategy")
    fig.tight_layout()
    fig.savefig(os.path.join(analysis_dir, "low_progress_by_strategy.png"), dpi=150)
    plt.close(fig)


def plot_role_changes_by_strategy(matches, analysis_dir, strategies):
    data = []
    for strategy in strategies:
        counts = []
        for row in matches:
            outcome = strategy_outcome_for_row(row, strategy)
            if outcome:
                counts.append(outcome["roleChanges"])
        data.append(counts)

    fig, ax = plt.subplots(figsize=(7, 4))
    ax.boxplot(data, tick_labels=strategies)
    ax.set_ylabel("total role changes across the team")
    ax.set_title("Role reassignment frequency by strategy")
    fig.tight_layout()
    fig.savefig(os.path.join(analysis_dir, "role_changes_by_strategy.png"), dpi=150)
    plt.close(fig)


def plot_coverage_vs_win_rate(matches, analysis_dir, strategies):
    fig, ax = plt.subplots(figsize=(7, 4.5))
    for strategy in strategies:
        xs, ys = [], []
        for row in matches:
            outcome = strategy_outcome_for_row(row, strategy)
            if outcome:
                xs.append(float(row["obstacleCoverageRatio"]))
                ys.append(1.0 if outcome["win"] else 0.0)
        if xs:
            ax.scatter(xs, ys, alpha=0.2, label=strategy)
    ax.set_xlabel("obstacleCoverageRatio")
    ax.set_ylabel("win (0/1)")
    ax.set_title("Map obstacle coverage vs. match outcome")
    ax.legend()
    fig.tight_layout()
    fig.savefig(os.path.join(analysis_dir, "coverage_vs_outcome.png"), dpi=150)
    plt.close(fig)


def parse_args():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--experiment-config", default=DEFAULT_CONFIG_PATH)
    parser.add_argument("--mapgen-executable", default="build/AstroPartyAI-mapgen")
    parser.add_argument("--headless-executable", default="build/AstroPartyAI-headless")
    parser.add_argument("--maps-dir", default="/tmp/astroparty_strategy_comparison_maps")
    parser.add_argument("--results-dir", default="experiments/results")
    parser.add_argument("--analysis-dir", default="experiments/analysis")
    parser.add_argument("--maps-per-family", type=int, default=None, help="Overrides the experiment config's value")
    return parser.parse_args()


def main():
    args = parse_args()
    experiment_config = load_experiment_config(args.experiment_config)
    strategies = experiment_config["strategies"]
    families = experiment_config["families"]
    maps_per_family = args.maps_per_family or experiment_config["mapsPerFamily"]
    seed_start = experiment_config["seedStart"]
    max_ticks = experiment_config["maxTicks"]
    simulation_config_path = experiment_config["simulationConfig"]
    generator_configs_dir = experiment_config["generatorConfigsDir"]

    os.makedirs(args.results_dir, exist_ok=True)
    os.makedirs(args.analysis_dir, exist_ok=True)

    maps_by_family = generate_maps(args.mapgen_executable, generator_configs_dir, args.maps_dir, families,
                                    maps_per_family, seed_start)
    total_maps = sum(len(maps) for maps in maps_by_family.values())
    print(f"Total maps: {total_maps}", file=sys.stderr)

    matches = run_all_matches(args.headless_executable, simulation_config_path, maps_by_family, strategies,
                               max_ticks)
    print(f"Total matches: {len(matches)}", file=sys.stderr)

    matches_csv_path = os.path.join(args.results_dir, "matches.csv")
    write_matches_csv(matches_csv_path, matches)

    summary_path = os.path.join(args.analysis_dir, "summary.csv")
    write_strategy_family_summary(summary_path, matches, strategies, families)
    write_pairwise_comparison(os.path.join(args.analysis_dir, "pairwise_comparison.csv"), matches, strategies,
                               families)

    plot_win_rate_by_family(summary_path, args.analysis_dir, strategies, families)
    plot_delivery_latency_by_strategy(matches, args.analysis_dir, strategies)
    plot_low_progress_by_strategy(matches, args.analysis_dir, strategies)
    plot_role_changes_by_strategy(matches, args.analysis_dir, strategies)
    plot_coverage_vs_win_rate(matches, args.analysis_dir, strategies)

    print(f"Wrote {matches_csv_path}, {summary_path}, pairwise_comparison.csv, and plots to {args.analysis_dir}",
          file=sys.stderr)
    return 0


if __name__ == "__main__":
    sys.exit(main())
