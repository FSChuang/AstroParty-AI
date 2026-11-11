# AstroParty-AI

![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)
[![Build & Test](https://github.com/FSChuang/AstroParty-AI/actions/workflows/ci.yml/badge.svg?branch=main)](https://github.com/FSChuang/AstroParty-AI/actions/workflows/ci.yml)

An extensible C++20 reimplementation of an *Astro Party*-style twin-stick
arcade shooter, built from the ground up as a platform for multi-agent game
AI research: strategic decision-making, procedural content generation, and
automated match evaluation.

## Overview

Ships can turn left or right, apply forward thrust, and brake. Releasing
the throttle lets the ship coast, so facing and travel direction can differ.
The team AI uses the same controls to steer around obstacles and slow down
near its targets.

The project is split cleanly into two halves that never depend on each
other in the wrong direction: a **simulation core** with zero dependency on
SDL2 or any rendering/input code, and a **presentation layer** (graphics,
keyboard input) built on top of it. The same simulation core also drives a
**headless match runner** with no window at all, which is what makes
automated AI-vs-AI evaluation possible in the first place.

## Current features

- Two-player local graphical **Classic Deathmatch** (two-way turning,
  thrust and braking, edge-triggered shooting, circular bullet/ship collision, win/draw
  detection, manual restart)
- A **Team Resource Battle** mode (3v3): two teams compete to collect Energy
  Core resources and deliver them to their own home region, with respawns,
  per-team scoring, and a configurable winning score / tick limit
- Circular arena **obstacles**: ships and bullets collide with them, spawn
  placement rejects overlapping positions, and a fixed 6-ship arena
  (`configs/maps/team_arena.json`) ships with the project
- A rule-based **3v3 AI module** (`ai/`) with offensive blue and defensive red
  policies. Both use shared steering and assign Collector, Defender,
  Interceptor, Escort, and Self-Defense roles from the current match state.
  A fixed-role baseline is available for comparison.
- A **seeded arena generator** with three size presets, configurable obstacle
  counts and resource placement, and a batch study for geometry and AI playability.
- A simulation library (`astroparty_sim`) with **no SDL dependency**,
  independently unit-tested
- `IShipController`: one shared action interface for human keyboard input,
  scripted controllers, and the rule-based AI — no special-casing in the
  simulation for "who" is controlling a ship, and no controller can mutate
  the simulation directly
- A headless executable (`AstroPartyAI-headless`) that plays a full Classic
  Deathmatch or Team Resource Battle match with no window and exports a
  structured JSON result, plus a stdlib-only Python script
  (`scripts/run_team_experiments.py`) for batch win-rate/score experiments
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
- **Python** (standard library only) — optional procedural arena generation and analysis of headless match results

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
| `--generate-map` | off | Generate a fresh arena and start team mode |
| `--map-size <small\|medium\|large>` | `medium` | Size preset for `--generate-map` |
| `--map-seed <integer>` | random | Replay a generated arena with `--generate-map` |
| `--team0-strategy <strategic\|red\|baseline>` | `strategic` | Team 0's AI |
| `--team1-strategy <strategic\|red\|baseline>` | `red` | Team 1's AI |

## Controls

| Player | Turn left | Turn right | Thrust | Brake | Shoot |
|---|---|---|---|---|---|
| Player 1 (blue) | `A` | `D` | `W` | `S` | Space |
| Player 2 (red) | Left Arrow | Right Arrow | Up Arrow | Down Arrow | `/` |

Thrust accelerates in the direction the ship faces. Braking slows the
ship along its current velocity and stops at zero. Opposing keys cancel.

Blue uses the offensive `strategic` policy. Red uses the `red` policy,
which screens its home side and escorts carriers from farther away. Add
`--team1-strategy strategic` for a mirror match or `--team1-strategy baseline`
for the fixed-role opponent.

Each strategic team can reserve one ship to support two collectors. That
ship escorts threatened allies before returning to its usual pursuit or
patrol. An unladen collector or defender switches to Self-Defense when an
enemy gets within 100 units, then resumes its task when the threat moves
away. Carriers evade and keep delivering. An orange diamond marks an
Interceptor, a purple square marks an Escort, and a white plus marks
Self-Defense.

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
{ "outcome": "Timeout", "winningShipId": null, "ticksElapsed": 3600 }
```

Team Resource Battle with the default blue and red policies:

```sh
./build/AstroPartyAI-headless \
  --config configs/team_resource_battle.json \
  --map configs/maps/team_arena.json \
  --team0-strategy strategic --team1-strategy red
```

```json
{
  "outcome": "TeamWon",
  "winningTeamId": 0,
  "ticksElapsed": 3463,
  "teamScores": [5, 4],
  "resourcesCollected": [6, 5],
  "shipsDestroyed": [1, 8]
}
```

| Flag | Default | Meaning |
|---|---|---|
| `--config <path>` | built-in defaults (Classic Deathmatch) | A `SimulationConfig` JSON file |
| `--map <path>` | built-in two-ship layout | A `MapDefinition` JSON file |
| `--max-ticks <N>` | Team match limit, or `3600` without one | Fixed-step budget before declaring a timeout |
| `--output <path>` | stdout | Where to write the JSON result |
| `--team0-strategy <strategic\|red\|baseline>` | `strategic` | Team 0's AI (Team Resource Battle only) |
| `--team1-strategy <strategic\|red\|baseline>` | `red` | Team 1's AI (Team Resource Battle only) |

Because this binary links only the simulation library — never SDL2 — it
runs anywhere, including CI containers, and can be invoked repeatedly by an
external script for batch experiments. `scripts/run_team_experiments.py`
(Python standard library only — it shells out to this executable and never
participates in the simulation itself) does exactly that: it runs a chosen
strategy pairing N times and reports win rates and average scores:

```sh
python3 scripts/run_team_experiments.py --runs 20 \
  --team0-strategy strategic --team1-strategy red --csv results.csv
```

Since the simulation has no randomness, repeated runs of an *identical*
config/map/strategy pairing are identical; vary the map, config, or
strategy pairing between runs to get a meaningful sample.

## Procedural Team Resource Battle arenas

To generate an arena and play it immediately, run from the repository root
with `python3` on your PATH (macOS or Linux):

```sh
./build/AstroPartyAI --generate-map --map-size small
./build/AstroPartyAI --generate-map --map-size large --map-seed 42
```

| Preset | Arena size | Default obstacles | Deliveries to win | Time limit with default config |
|---|---|---:|---:|---|
| Small | 960 × 540 | 2 | 3 | 60 seconds |
| Medium (default) | 1280 × 720 | 4 | 5 | 60 seconds |
| Large | 1600 × 900 | 6 | 7 | 90 seconds |

Generated maps store the delivery target in their JSON. Large maps also
store the longer time limit, giving ships time for the extra trips.
These rules apply when the map is saved and loaded through `--map`.
Maps without a rule field use the corresponding simulation config setting.

Each unseeded launch picks a fresh seed and prints it to the terminal.
Pressing `R` restarts the match on the current arena. Launch again for a
new layout, or reuse the seed and size to replay that arena.

To save a map, use the Python generator with `--size` and then load it:

```sh
python3 -B scripts/generate_team_arena.py --size medium --seed 1 --obstacles 4 --resources 5 \
  --resource-layout central --output out/generated-arena.json
./build/AstroPartyAI --mode team --map out/generated-arena.json
```

The generator uses Python's standard library. Use `--obstacles` to
override the preset with an even count from 2 to 12. Resource counts can
be 3 through 9. `central`
puts resource pairs nearer the midline, and `spread` moves them toward
the two home regions. Features are paired under 180° rotation, with an
additional center resource for odd counts. The seed and parameters
reproduce the layout. Single-map generation rejects placement or geometry
failures. The batch study also checks delivery behavior.

```sh
python3 -B tests/test_generate_team_arena.py
python3 -B scripts/study_team_arenas.py --output out/procedural-arenas
python3 -B scripts/study_team_arenas.py --output out/procedural-arenas --plots-only
```

The study evaluates 50 primary seeds per family on medium maps with five
resources (4 or 8 obstacles, with central or spread placement). A candidate
passes geometry if spawns, home centers, and resources are clear and connected
on a 20-unit navigation grid with ship clearance. The AI check runs two headless
matches per candidate, swapping the `strategic` and `red` policies for the
second match. Both matches must finish, and each arena side must score in at
least one of the two matches. Use `--team1-strategy baseline` to repeat the
earlier baseline comparison. The first 30 distinct passing layouts per family
make up the distribution sample. `study.json` records every attempt, the
strategy assignments and match results, and the accepted sample paths. The
three SVG histograms show coverage, resource location, and delivery detours. The method of
comparing distributions across settings follows
[Smith and Whitehead's expressive-range analysis](https://www.pcgworkshop.com/archive/smith2010analyzing.pdf).

To capture the first frame after a delivery while a team match is still
running, use the graphical executable with `--capture-frame`. It writes a
BMP and prints the tick and score before exiting.

```sh
./build/AstroPartyAI --mode team \
  --map out/procedural-arenas/maps/arena-4-5-central-1.json \
  --capture-frame out/procedural-arenas/arena-4-5-central-1.bmp
```

**Controllers are the extension point for AI.** Any class implementing
`input::IShipController` — human, scripted, or a future learning agent —
produces the same `ShipAction` the simulation consumes; nothing in the
simulation, headless runner, or graphical app needs to change to add a new
kind of controller. See `docs/ARCHITECTURE.md` for the interface and its
guarantees (a controller can observe the full match state but has no path
to mutate it directly).

## Architecture overview

```
   astroparty_sim (no SDL)                    astroparty_platform (SDL)
   ├─ core/      World, GameMode,                   ├─ rendering/  SDL window,
   │             SimulationConfig, MapDefinition,    │              renderer, drawing
   │             MatchState, TeamMatchState          └─ input/      Keyboard controller
   ├─ entities/  Ship, Bullet, Resource
   ├─ physics/   Arena bounds, collision, obstacles
   ├─ ai/        Steering, unit behaviors,
   │             BaselineTeamStrategy, UtilityTeamStrategy, RedTeamStrategy,
   │             TeamShipController
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
- [x] **Milestone 4** — Seeded procedural arenas, geometry and AI validation, and analysis of generated maps
- [ ] Reinforcement learning for agents

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
