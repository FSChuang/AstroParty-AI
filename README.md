# AstroParty-AI

![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)
[![Build & Test](https://github.com/FSChuang/AstroParty-AI/actions/workflows/ci.yml/badge.svg?branch=main)](https://github.com/FSChuang/AstroParty-AI/actions/workflows/ci.yml)

An extensible C++20 reimplementation of an *Astro Party*-style twin-stick
arcade shooter, built from the ground up as a platform for multi-agent game
AI research: strategic decision-making, procedural content generation, and
automated match evaluation.

## Overview

In AstroParty-AI, each ship accelerates continuously — there is no "stop" or
"reverse" control. The only way to survive is to steer (ships turn one
direction only) while lining up a shot on an opponent. That one constraint
is the entire skill ceiling of the game, for both human players and AI
controllers.

The project is split cleanly into two halves that never depend on each
other in the wrong direction: a **simulation core** with zero dependency on
SDL2 or any rendering/input code, and a **presentation layer** (graphics,
keyboard input) built on top of it. The same simulation core also drives a
**headless match runner** with no window at all, which is what makes
automated AI-vs-AI evaluation possible in the first place.

## Current features

- Two-player local graphical **Classic Deathmatch** (continuous acceleration,
  turning, edge-triggered shooting, circular bullet/ship collision, win/draw
  detection, manual restart)
- A **Team Resource Battle** mode (3v3): two teams compete to collect Energy
  Core resources and deliver them to their own home region, with respawns,
  per-team scoring, and a configurable winning score / tick limit
- Circular arena **obstacles**: ships and bullets collide with them, spawn
  placement rejects overlapping positions, and a fixed 6-ship arena
  (`configs/maps/team_arena.json`) ships with the project
- A **procedural map generator** (`procgen/`, `AstroPartyAI-mapgen`):
  parameterized, seeded, reproducible generation of Team Resource Battle
  maps in three families (open / clustered / chokepoint — see
  `configs/procgen/`), an automatic structural + geometric validity
  checker with bounded obstacle-removal repair, and a stdlib+matplotlib
  analysis script (`scripts/analyze_generated_maps.py`) — see
  [`docs/PROCEDURAL_GENERATION.md`](docs/PROCEDURAL_GENERATION.md)
- A rule-based **3v3 AI module** (`ai/`) with **four selectable team
  strategies** sharing the same steering/movement primitives: `baseline`
  (simpler fixed-role opponent), `strategic` (utility-based, dynamically
  assigns Collector / Defender / Interceptor / Escort roles every tick),
  `fixed` (a literally fixed role-by-index assignment, never reassigned),
  and `hysteresis` (`strategic`'s same logic, plus role commitment to
  resist threshold oscillation) — no neural networks, no learned
  policies, fully deterministic; see
  [`docs/AI_EVALUATION.md`](docs/AI_EVALUATION.md)
- A **match diagnostics** module (`diagnostics/`): precisely-defined,
  per-ship measurements (low-progress ticks, shots fired, obstacle
  contacts, role changes, resource delivery latency) for any Team
  Resource Battle match, exposed via `AstroPartyAI-headless --diagnostics`
- A simulation library (`astroparty_sim`) with **no SDL dependency**,
  independently unit-tested
- `IShipController`: one shared action interface for human keyboard input,
  scripted controllers, and the rule-based AI — no special-casing in the
  simulation for "who" is controlling a ship, and no controller can mutate
  the simulation directly
- A headless executable (`AstroPartyAI-headless`) that plays a full Classic
  Deathmatch or Team Resource Battle match with no window and exports a
  structured JSON result (optionally including the full diagnostics
  above), plus two stdlib-only Python scripts
  (`scripts/run_team_experiments.py`, `scripts/run_strategy_comparison.py`)
  for batch win-rate/score experiments and a 4-strategy, 3-map-family
  comparison study (`experiments/`)
- Deterministic, reproducible simulation: no randomness anywhere in the
  core loop, verified by automated replay tests
- JSON-loadable, validated simulation configuration (`SimulationConfig`)
  and arena/map definitions (`MapDefinition`)
- Continuous integration building and testing on both Ubuntu and macOS on
  every push and pull request

See the [Roadmap](#roadmap) below for what's in progress or planned next.

## Technology stack

- **C++20** — all gameplay, simulation, and physics code
- **SDL2** — windowing, rendering, and keyboard input (presentation layer only)
- **CMake** (≥ 3.21) — build system
- **nlohmann/json** — configuration and map file parsing (fetched via CMake `FetchContent`)
- **doctest** — unit testing, run through CTest (fetched via CMake `FetchContent`)
- **GitHub Actions** — CI on Ubuntu and macOS
- **Python** (standard library only) — offline analysis of headless match results; never part of the runtime simulation

## Installation / build

### Requirements

- A C++20 compiler (CI builds with GCC on Ubuntu and Apple Clang on macOS)
- CMake 3.21+
- SDL2 development headers and libraries
- Internet access on first configure (to fetch doctest and nlohmann/json)

```sh
# Install SDL2
brew install sdl2                      # macOS
sudo apt-get install libsdl2-dev       # Debian/Ubuntu
sudo dnf install SDL2-devel            # Fedora
```

### Build

```sh
cmake -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build -j
```

This produces `build/AstroPartyAI` (graphical game), `build/AstroPartyAI-headless`
(no-window match runner), and `build/tests/astroparty_tests`.

If CMake can't find SDL2 automatically (common on Apple Silicon with
Homebrew), point it there explicitly:

```sh
cmake -B build -DCMAKE_PREFIX_PATH="$(brew --prefix sdl2)"
```

Useful CMake options: `ASTROPARTY_BUILD_TESTS` (default `ON`),
`ASTROPARTY_WARNINGS_AS_ERRORS` (default `OFF`),
`ASTROPARTY_ENABLE_SANITIZERS` (default `OFF`, builds with ASan+UBSan).

## How to play

```sh
./build/AstroPartyAI
```

Run from the repository root. By default this starts a two-player Classic
Deathmatch.

To instead *observe* a 3v3 Team Resource Battle AI match (no human
controller — this mode has no keyboard input):

```sh
./build/AstroPartyAI --mode team
```

| Flag | Default | Meaning |
|---|---|---|
| `--mode <classic\|team>` | `classic` | Which game mode to run |
| `--config <path>` | `configs/team_resource_battle.json` | `SimulationConfig` for `--mode team` |
| `--map <path>` | `configs/maps/team_arena.json` | `MapDefinition` for `--mode team` |
| `--team0-strategy <name>` | `strategic` | Team 0's AI — `strategic`, `baseline`, `fixed`, or `hysteresis` |
| `--team1-strategy <name>` | `baseline` | Team 1's AI — same choices |

## Controls

| Player | Turn | Shoot |
|---|---|---|
| Player 1 (blue) | `D` | `W` |
| Player 2 (red) | Right Arrow | Up Arrow |

(`--mode team` has no human-controlled ships; both teams are AI.)

| Action | Key |
|---|---|
| Restart the match | `R` |
| Quit | `Esc` or close the window |

The current match outcome (and, in Team Resource Battle, the live score) is
shown in the window title bar.

## Headless execution and AI experiments

`AstroPartyAI-headless` plays one complete match with no SDL window and
prints a JSON result — the building block for batch AI evaluation. The
mode is whatever `--config`'s `SimulationConfig.mode` selects.

Classic Deathmatch:

```sh
./build/AstroPartyAI-headless --max-ticks 3600
```

```json
{ "outcome": "ShipWon", "winningShipId": 0, "ticksElapsed": 212 }
```

Team Resource Battle (strategic AI vs. baseline AI, 3v3):

```sh
./build/AstroPartyAI-headless \
  --config configs/team_resource_battle.json \
  --map configs/maps/team_arena.json \
  --team0-strategy strategic --team1-strategy baseline
```

```json
{
  "outcome": "TeamWon",
  "winningTeamId": 0,
  "ticksElapsed": 3600,
  "teamScores": [2, 1],
  "resourcesCollected": [3, 2],
  "shipsDestroyed": [0, 1]
}
```

| Flag | Default | Meaning |
|---|---|---|
| `--config <path>` | built-in defaults (Classic Deathmatch) | A `SimulationConfig` JSON file |
| `--map <path>` | built-in two-ship layout | A `MapDefinition` JSON file |
| `--max-ticks <N>` | `3600` | Fixed-step budget before declaring a timeout |
| `--output <path>` | stdout | Where to write the JSON result |
| `--team0-strategy <name>` | `strategic` | Team 0's AI (Team Resource Battle only) — `strategic`, `baseline`, `fixed`, or `hysteresis` |
| `--team1-strategy <name>` | `baseline` | Team 1's AI (Team Resource Battle only) — same choices |
| `--diagnostics` | off | Team Resource Battle only: emit `{"result": ..., "diagnostics": ...}` with per-ship AI diagnostics (see `docs/AI_EVALUATION.md`) instead of the bare result |

Because this binary links only the simulation library — never SDL2 — it
runs anywhere, including CI containers, and can be invoked repeatedly by an
external script for batch experiments. `scripts/run_team_experiments.py`
(Python standard library only — it shells out to this executable and never
participates in the simulation itself) does exactly that: it runs a chosen
strategy pairing N times and reports win rates and average scores:

```sh
python3 scripts/run_team_experiments.py --runs 20 \
  --team0-strategy strategic --team1-strategy baseline --csv results.csv
```

Since the simulation has no randomness, repeated runs of an *identical*
config/map/strategy pairing are identical; vary the map, config, or
strategy pairing between runs to get a meaningful sample.

**Controllers are the extension point for AI.** Any class implementing
`input::IShipController` — human, scripted, or a future learning agent —
produces the same `ShipAction` the simulation consumes; nothing in the
simulation, headless runner, or graphical app needs to change to add a new
kind of controller. See `docs/ARCHITECTURE.md` for the interface and its
guarantees (a controller can observe the full match state but has no path
to mutate it directly).

## AI strategy comparison (Milestone 5)

`scripts/run_strategy_comparison.py` reproduces the full study: generates
20 procedurally generated maps per family (open/clustered/chokepoint) and
runs every pairing of `strategic`/`fixed`/`hysteresis`, both side
assignments, on each one (360 matches total, subprocess calls only — no
gameplay or AI logic reimplemented in Python):

```sh
python3 scripts/run_strategy_comparison.py \
  --mapgen-executable build/AstroPartyAI-mapgen \
  --headless-executable build/AstroPartyAI-headless
```

Writes `experiments/results/matches.csv` (one row per match) and
`experiments/analysis/` (win-rate/pairwise-comparison CSVs and plots,
bootstrapped over maps — not over individual matches, since the two
side-swapped matches per map are a paired observation, not two
independent samples). See [`docs/AI_EVALUATION.md`](docs/AI_EVALUATION.md)
for the AI architecture audit, the diagnostics that motivated each new
strategy, and the actual results.

## Procedural map generation

`AstroPartyAI-mapgen` generates a batch of accepted Team Resource Battle
maps, directly loadable by the graphical client, the headless runner, and
`World` itself:

```sh
./build/AstroPartyAI-mapgen --config configs/procgen/chokepoint.json \
  --seed 0 --count 20 --output-dir /tmp/my_maps
```

| Flag | Default | Meaning |
|---|---|---|
| `--config <path>` | required | A `MapGeneratorConfig` JSON file (see `configs/procgen/`) |
| `--seed <N>` | required | First seed; subsequent attempts use `seed+1`, `seed+2`, ... |
| `--count <N>` | required | How many accepted maps to produce |
| `--output-dir <dir>` | required | Where generated map JSON files and `manifest.csv` are written |
| `--label <name>` | the config's filename | Tag written into `manifest.csv` and used in output filenames |
| `--max-attempts <N>` | `count * 20` | Bounded attempt budget — never loops unboundedly |

Prints `GenerationStats` (raw validity rate, post-repair acceptance rate,
...) as JSON to stdout. See
[`docs/PROCEDURAL_GENERATION.md`](docs/PROCEDURAL_GENERATION.md) for the
generation algorithm, the three map families (open/clustered/chokepoint —
the same generator, different config values), the structural + geometric
validity checker, the bounded repair strategy, and the full
expressive-range analysis (`scripts/analyze_generated_maps.py`,
`docs/procgen_analysis/`).

## Architecture overview

```
   astroparty_sim (no SDL)                    astroparty_platform (SDL)
   ├─ core/      World, GameMode,                   ├─ rendering/  SDL window,
   │             SimulationConfig, MapDefinition,    │              renderer, drawing
   │             MatchState, TeamMatchState          └─ input/      Keyboard controller
   ├─ entities/  Ship, Bullet, Resource
   ├─ physics/   Arena bounds, collision, obstacles
   ├─ ai/        Steering, unit behaviors, TeamShipController,
   │             Baseline/Utility/FixedRole/Hysteresis TeamStrategy
   ├─ procgen/   MapGenerator, MapValidity, MapRepair,
   │             GenerationPipeline, MapMetrics
   ├─ diagnostics/ MatchDiagnostics (instrumented match runner)
   └─ headless/  MatchRunner/MatchResult,
                 TeamMatchRunner/TeamMatchResult
            │                                                 │
            └───────────────────────┬─────────────────────────┘
                                     ▼
              Application (graphical) / headless_main (no window)
```

The simulation never imports anything from `rendering/` or SDL; the
graphical and headless entry points each compose the same simulation with a
different presentation layer on top. Full details, including the
determinism guarantees and the provenance of each physics constant, are in
[`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md).

## Testing

```sh
cmake -B build
cmake --build build -j
ctest --test-dir build --output-on-failure
```

The simulation has no SDL dependency, so the entire suite runs with no
window, no display, and no input device — which is also exactly what makes
it safe to run in CI. See `docs/ARCHITECTURE.md` for current test/assertion
counts and sanitizer usage.

## Roadmap

- [x] **Milestone 1** — Minimal playable two-player graphical game, simulation/rendering separation
- [x] **Milestone 2** — Shared controller interface, headless execution, deterministic simulation, JSON configuration and map loading
- [x] **Milestone 3** — Functional obstacles, a Team Resource Battle mode (3v3), rule-based unit AI, and a strategic team-AI layer evaluated against a baseline
- [x] **Milestone 4** — Parameterized procedural map generation (3 map families), an automatic structural/geometric validity checker with bounded repair, and an expressive-range analysis of 200+ generated maps
- [x] **Milestone 5** — Per-ship AI diagnostics, two new team strategies (fixed-role and utility-with-hysteresis) compared against the existing baseline/strategic AI across 360 matches on 60 generated maps
- [ ] **Milestone 6** — Reinforcement-learning-based agents; wider-scale experiments

## License and acknowledgments

AstroParty-AI is licensed under the [MIT License](LICENSE).

This project is a clean-room C++20 implementation, informed by (but
containing no source code from) [CAstroParty](https://github.com/kkysen/CAstroParty),
a C/SDL2 Astro Party clone used as an architecture reference during initial
design. See [`THIRD_PARTY_NOTICES.md`](THIRD_PARTY_NOTICES.md) for its
license and a full provenance statement, and
[`docs/UPSTREAM_AUDIT.md`](docs/UPSTREAM_AUDIT.md) for the architecture
audit that informed this project's early design decisions. AstroParty-AI
began as a project for a university Game AI course; it is maintained here
as a standalone, extensible game and AI research platform.
