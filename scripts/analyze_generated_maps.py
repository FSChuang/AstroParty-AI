#!/usr/bin/env python3
"""Runs AstroPartyAI-mapgen across the three map families plus two
parameter-sensitivity sweeps, then summarizes and plots the result.

This script orchestrates subprocess calls to the already-built C++
generator (AstroPartyAI-mapgen) and performs purely OFFLINE statistical
analysis/plotting on the metrics it reports -- it does not reimplement
any generation, validity-checking, or geometry logic, and it never
participates in the simulation itself (same rule
scripts/run_team_experiments.py already follows). matplotlib is used for
plotting per the Milestone 4 spec; everything else is the standard
library.

Reproduction:
    python3 scripts/analyze_generated_maps.py \\
        --executable build/AstroPartyAI-mapgen \\
        --configs-dir configs/procgen \\
        --maps-output-dir /tmp/astroparty_procgen_maps \\
        --analysis-dir docs/procgen_analysis \\
        --count-per-family 40

Generated map files themselves are written under --maps-output-dir
(scratch space, not committed); only the combined per-map metrics CSV and
the plots go under --analysis-dir, which IS meant to be committed.
"""
import argparse
import csv
import json
import os
import statistics
import subprocess
import sys
import tempfile

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

METRIC_FIELDS = [
    "obstacleCoverageRatio", "team0NavigationPathLength", "team1NavigationPathLength",
    "teamAccessibilityImbalance", "obstacleClusterCount", "reachableFreeSpaceFraction",
]


def run_mapgen(executable, config_path, seed, count, output_dir, label, max_attempts=None):
    command = [
        executable,
        "--config", config_path,
        "--seed", str(seed),
        "--count", str(count),
        "--output-dir", output_dir,
        "--label", label,
    ]
    if max_attempts is not None:
        command += ["--max-attempts", str(max_attempts)]
    completed = subprocess.run(command, capture_output=True, text=True, check=True)
    stats = json.loads(completed.stdout)

    rows = []
    manifest_path = os.path.join(output_dir, "manifest.csv")
    with open(manifest_path, newline="") as manifest_file:
        for row in csv.DictReader(manifest_file):
            rows.append(row)
    return stats, rows


def run_family_study(executable, configs_dir, families, count_per_family, seed_start, maps_output_dir):
    all_rows = []
    stats_by_family = {}
    for family in families:
        config_path = os.path.join(configs_dir, f"{family}.json")
        output_dir = os.path.join(maps_output_dir, "family_study", family)
        stats, rows = run_mapgen(executable, config_path, seed_start, count_per_family, output_dir, family)
        stats_by_family[family] = stats
        for row in rows:
            row["experiment"] = "family_study"
            all_rows.append(row)
        print(f"[family_study] {family}: accepted={stats['accepted']} "
              f"rawValidityRate={stats['rawValidityRate']:.3f} "
              f"postRepairAcceptanceRate={stats['postRepairAcceptanceRate']:.3f}", file=sys.stderr)
    return all_rows, stats_by_family


def run_density_sensitivity(executable, base_config_path, count_per_level, seed_start, maps_output_dir):
    with open(base_config_path) as f:
        base_config = json.load(f)

    levels = {"low": 4, "medium": 14, "high": 26}
    all_rows = []
    stats_by_level = {}
    with tempfile.TemporaryDirectory() as temp_dir:
        for level_name, obstacle_count in levels.items():
            config = dict(base_config)
            config["obstacleCount"] = obstacle_count
            config["clusterCount"] = 0
            config_path = os.path.join(temp_dir, f"density_{level_name}.json")
            with open(config_path, "w") as f:
                json.dump(config, f)

            output_dir = os.path.join(maps_output_dir, "density_sensitivity", level_name)
            label = f"density_{level_name}"
            stats, rows = run_mapgen(executable, config_path, seed_start, count_per_level, output_dir, label)
            stats_by_level[level_name] = stats
            for row in rows:
                row["experiment"] = "density_sensitivity"
                row["densityLevel"] = level_name
                all_rows.append(row)
            print(f"[density_sensitivity] {level_name} (obstacleCount={obstacle_count}): "
                  f"accepted={stats['accepted']} rawValidityRate={stats['rawValidityRate']:.3f}", file=sys.stderr)
    return all_rows, stats_by_level


def run_symmetry_sensitivity(executable, base_config_path, count_per_variant, seed_start, maps_output_dir):
    with open(base_config_path) as f:
        base_config = json.load(f)

    all_rows = []
    stats_by_variant = {}
    with tempfile.TemporaryDirectory() as temp_dir:
        for symmetric in (True, False):
            variant_name = "symmetric" if symmetric else "asymmetric"
            config = dict(base_config)
            config["symmetric"] = symmetric
            config_path = os.path.join(temp_dir, f"{variant_name}.json")
            with open(config_path, "w") as f:
                json.dump(config, f)

            output_dir = os.path.join(maps_output_dir, "symmetry_sensitivity", variant_name)
            stats, rows = run_mapgen(executable, config_path, seed_start, count_per_variant, output_dir, variant_name)
            stats_by_variant[variant_name] = stats
            for row in rows:
                row["experiment"] = "symmetry_sensitivity"
                row["symmetryVariant"] = variant_name
                all_rows.append(row)
            print(f"[symmetry_sensitivity] {variant_name}: accepted={stats['accepted']} "
                  f"rawValidityRate={stats['rawValidityRate']:.3f}", file=sys.stderr)
    return all_rows, stats_by_variant


def write_combined_csv(path, rows):
    if not rows:
        return
    fieldnames = sorted({key for row in rows for key in row.keys()})
    with open(path, "w", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=fieldnames)
        writer.writeheader()
        writer.writerows(rows)


def write_summary_csv(path, rows, group_field):
    groups = {}
    for row in rows:
        groups.setdefault(row[group_field], []).append(row)

    with open(path, "w", newline="") as f:
        fieldnames = [group_field, "count"] + [f"{m}_mean" for m in METRIC_FIELDS] + [f"{m}_stdev" for m in METRIC_FIELDS]
        writer = csv.DictWriter(f, fieldnames=fieldnames)
        writer.writeheader()
        for group_value, group_rows in groups.items():
            summary_row = {group_field: group_value, "count": len(group_rows)}
            for metric in METRIC_FIELDS:
                values = [float(row[metric]) for row in group_rows]
                summary_row[f"{metric}_mean"] = statistics.fmean(values) if values else 0.0
                summary_row[f"{metric}_stdev"] = statistics.pstdev(values) if len(values) > 1 else 0.0
            writer.writerow(summary_row)


def plot_family_histograms(rows, families, analysis_dir):
    for metric in ("obstacleCoverageRatio", "obstacleClusterCount"):
        fig, ax = plt.subplots(figsize=(7, 4))
        for family in families:
            values = [float(row[metric]) for row in rows if row.get("label") == family]
            if values:
                ax.hist(values, bins=12, alpha=0.5, label=family)
        ax.set_xlabel(metric)
        ax.set_ylabel("count")
        ax.set_title(f"{metric} distribution by map family")
        ax.legend()
        fig.tight_layout()
        fig.savefig(os.path.join(analysis_dir, f"family_{metric}_histogram.png"), dpi=150)
        plt.close(fig)


def plot_family_comparison_boxplot(rows, families, analysis_dir, metric="teamAccessibilityImbalance"):
    fig, ax = plt.subplots(figsize=(7, 4))
    data = [[float(row[metric]) for row in rows if row.get("label") == family] for family in families]
    ax.boxplot(data, tick_labels=families)
    ax.set_ylabel(metric)
    ax.set_title(f"{metric} across map families")
    fig.tight_layout()
    fig.savefig(os.path.join(analysis_dir, f"family_{metric}_boxplot.png"), dpi=150)
    plt.close(fig)


def plot_density_sensitivity(rows, analysis_dir):
    levels = ["low", "medium", "high"]
    data = [[float(row["obstacleCoverageRatio"]) for row in rows if row.get("densityLevel") == level]
            for level in levels]
    fig, ax = plt.subplots(figsize=(7, 4))
    ax.boxplot(data, tick_labels=levels)
    ax.set_ylabel("obstacleCoverageRatio")
    ax.set_title("Parameter sensitivity: obstacleCount level vs. resulting coverage")
    fig.tight_layout()
    fig.savefig(os.path.join(analysis_dir, "density_sensitivity_coverage.png"), dpi=150)
    plt.close(fig)


def plot_symmetry_sensitivity(rows, analysis_dir):
    variants = ["symmetric", "asymmetric"]
    data = [[float(row["teamAccessibilityImbalance"]) for row in rows if row.get("symmetryVariant") == variant]
            for variant in variants]
    fig, ax = plt.subplots(figsize=(6, 4))
    ax.boxplot(data, tick_labels=variants)
    ax.set_ylabel("teamAccessibilityImbalance")
    ax.set_title("Symmetric vs. asymmetric generation: accessibility imbalance")
    fig.tight_layout()
    fig.savefig(os.path.join(analysis_dir, "symmetry_sensitivity_imbalance.png"), dpi=150)
    plt.close(fig)


def plot_raw_validity_rates(all_stats, analysis_dir):
    labels = list(all_stats.keys())
    raw_rates = [all_stats[k]["rawValidityRate"] for k in labels]
    accept_rates = [all_stats[k]["postRepairAcceptanceRate"] for k in labels]

    x = range(len(labels))
    width = 0.35
    fig, ax = plt.subplots(figsize=(9, 4))
    ax.bar([i - width / 2 for i in x], raw_rates, width, label="raw validity rate")
    ax.bar([i + width / 2 for i in x], accept_rates, width, label="post-repair acceptance rate")
    ax.set_xticks(list(x))
    ax.set_xticklabels(labels, rotation=30, ha="right")
    ax.set_ylabel("rate")
    ax.set_title("Raw validity vs. post-repair acceptance by generation run")
    ax.legend()
    fig.tight_layout()
    fig.savefig(os.path.join(analysis_dir, "raw_validity_rates.png"), dpi=150)
    plt.close(fig)


def parse_args():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--executable", default="build/AstroPartyAI-mapgen")
    parser.add_argument("--configs-dir", default="configs/procgen")
    parser.add_argument("--families", default="open,clustered,chokepoint")
    parser.add_argument("--count-per-family", type=int, default=40)
    parser.add_argument("--density-count-per-level", type=int, default=15)
    parser.add_argument("--symmetry-count-per-variant", type=int, default=20)
    parser.add_argument("--seed-start", type=int, default=0)
    parser.add_argument("--maps-output-dir", required=True, help="Scratch directory for generated map files (not committed)")
    parser.add_argument("--analysis-dir", default="docs/procgen_analysis", help="Where the combined CSV and plots are written")
    return parser.parse_args()


def main():
    args = parse_args()
    families = args.families.split(",")
    os.makedirs(args.analysis_dir, exist_ok=True)

    all_rows = []
    all_stats = {}

    family_rows, family_stats = run_family_study(args.executable, args.configs_dir, families,
                                                  args.count_per_family, args.seed_start, args.maps_output_dir)
    all_rows += family_rows
    all_stats.update({f"family:{k}": v for k, v in family_stats.items()})

    base_config_path = os.path.join(args.configs_dir, "open.json")
    density_rows, density_stats = run_density_sensitivity(args.executable, base_config_path,
                                                           args.density_count_per_level, args.seed_start,
                                                           args.maps_output_dir)
    all_rows += density_rows
    all_stats.update({f"density:{k}": v for k, v in density_stats.items()})

    symmetry_rows, symmetry_stats = run_symmetry_sensitivity(args.executable, base_config_path,
                                                              args.symmetry_count_per_variant, args.seed_start,
                                                              args.maps_output_dir)
    all_rows += symmetry_rows
    all_stats.update({f"symmetry:{k}": v for k, v in symmetry_stats.items()})

    write_combined_csv(os.path.join(args.analysis_dir, "all_maps_metrics.csv"), all_rows)
    write_summary_csv(os.path.join(args.analysis_dir, "family_summary.csv"), family_rows, "label")

    with open(os.path.join(args.analysis_dir, "generation_stats.json"), "w") as f:
        json.dump(all_stats, f, indent=2)

    plot_family_histograms(family_rows, families, args.analysis_dir)
    plot_family_comparison_boxplot(family_rows, families, args.analysis_dir)
    plot_density_sensitivity(density_rows, args.analysis_dir)
    plot_symmetry_sensitivity(symmetry_rows, args.analysis_dir)
    plot_raw_validity_rates(all_stats, args.analysis_dir)

    print(f"\nTotal accepted maps across all runs: {len(all_rows)}", file=sys.stderr)
    print(f"Wrote combined metrics CSV, family summary CSV, generation_stats.json, and plots to {args.analysis_dir}",
          file=sys.stderr)
    return 0


if __name__ == "__main__":
    sys.exit(main())
