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

- Two-player local graphical gameplay (continuous acceleration, turning,
  edge-triggered shooting, circular bullet/ship collision, win/draw
  detection, manual restart)
- A simulation library (`astroparty_sim`) with **no SDL dependency**,
  independently unit-tested
- `IShipController`: one shared action interface for human keyboard input
  and scripted/AI controllers — no special-casing in the simulation for
  "who" is controlling a ship
- A headless executable (`AstroPartyAI-headless`) that plays a full match
  with no window and exports a structured JSON result
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

Run it from anywhere — the game has no dependency on the current working
directory.

## Controls

| Player | Turn | Shoot |
|---|---|---|
| Player 1 (blue) | `D` | `W` |
| Player 2 (red) | Right Arrow | Up Arrow |

| Action | Key |
|---|---|
| Restart the match | `R` |
| Quit | `Esc` or close the window |

The current match outcome is shown in the window title bar.

## Headless execution and AI experiments

`AstroPartyAI-headless` plays one complete match with no SDL window and
prints a JSON result — the building block for batch AI evaluation:

```sh
./build/AstroPartyAI-headless --max-ticks 3600
```

```json
{ "outcome": "ShipWon", "winningShipId": 0, "ticksElapsed": 212 }
```

| Flag | Default | Meaning |
|---|---|---|
| `--config <path>` | built-in defaults | A `SimulationConfig` JSON file |
| `--map <path>` | built-in two-ship layout | A `MapDefinition` JSON file |
| `--max-ticks <N>` | `3600` | Fixed-step budget before declaring a timeout |
| `--output <path>` | stdout | Where to write the JSON result |

Because this binary links only the simulation library — never SDL2 — it
runs anywhere, including CI containers, and can be invoked repeatedly by an
external script for batch experiments.

**Controllers are the extension point for AI.** Any class implementing
`input::IShipController` — human, scripted, or a future learning agent —
produces the same `ShipAction` the simulation consumes; nothing in the
simulation, headless runner, or graphical app needs to change to add a new
kind of controller. See `docs/ARCHITECTURE.md` for the interface and its
guarantees (a controller can observe the full match state but has no path
to mutate it directly).

## Architecture overview

```
   astroparty_sim (no SDL)              astroparty_platform (SDL)
   ├─ core/      World, SimulationConfig,     ├─ rendering/  SDL window,
   │             MapDefinition, MatchState    │              renderer, drawing
   ├─ entities/  Ship, Bullet                 └─ input/      Keyboard controller
   ├─ physics/   Arena bounds, collision
   └─ headless/  MatchRunner, MatchResult
            │                                         │
            └───────────────┬─────────────────────────┘
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
- [ ] **Milestone 3** — Functional obstacles, a Team Resource Battle mode (3v3), rule-based unit AI, and a strategic team-AI layer evaluated against a baseline
- [ ] **Milestone 4** — Procedural arena generation

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
