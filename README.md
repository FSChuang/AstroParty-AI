# AstroParty-AI

![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)
[![Build & Test](https://github.com/FSChuang/AstroParty-AI/actions/workflows/ci.yml/badge.svg?branch=main)](https://github.com/FSChuang/AstroParty-AI/actions/workflows/ci.yml)

> This repository is currently **private**. The Build & Test badge above only
> renders for people with repository access (a GitHub limitation, not a
> broken badge) — see "Continuous integration" below.

A from-scratch C++20 reimplementation of an Astro Party-style twin-stick
arcade shooter, built as an extensible platform for future multi-agent game
AI experiments (strategic decision-making, procedural content generation,
and automated evaluation).

This is **Milestone 2**: the graphical game from Milestone 1 (playable,
manually verified) plus the architecture Milestone 3's AI work needs —
a controller interface shared by humans and scripts, a headless match
runner, externalized configuration, and basic map loading. No strategic
team AI, procedural generation, or Python tooling exists yet — see
"Planned AI extensions" below.

## Project overview

AstroParty-AI is a clean-room C++20 rewrite, informed by (but not copied
from) [CAstroParty](https://github.com/kkysen/CAstroParty), a C/SDL2
reference implementation of the same game. A full audit of that reference
project — its architecture, build/runtime behavior, and a component-by-component
migration plan — lives in `astro-party-workspace/audit/`. See
`THIRD_PARTY_NOTICES.md` for the upstream license and provenance record, and
`docs/ARCHITECTURE.md` for how this project is organized and why.

## Technology stack

- **C++20** for all gameplay, simulation, and physics code
- **SDL2** for windowing, rendering, and keyboard input
- **CMake** (>= 3.21) as the build system
- **nlohmann/json** (fetched via CMake `FetchContent`) for configuration and map files
- **doctest** (fetched via CMake `FetchContent`) for unit testing, run through **CTest**
- **GitHub Actions** for continuous integration (Ubuntu + macOS)
- **Python** (planned, not yet used) for future evaluation and ML experiments

## Build requirements

- A C++20 compiler (tested with Apple Clang 17 on macOS and GCC on Ubuntu,
  including in CI — see "Continuous integration")
- CMake 3.21 or newer
- SDL2 development headers and libraries
- Internet access the first time you configure the project (to fetch
  doctest and nlohmann/json for the test suite; see "Building without
  network access" below)

### Installing SDL2

```sh
# macOS (Homebrew)
brew install sdl2

# Debian/Ubuntu
sudo apt-get install libsdl2-dev

# Fedora
sudo dnf install SDL2-devel
```

## Build instructions

```sh
cmake -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build -j
```

This produces three binaries under `build/`:

- `AstroPartyAI` — the graphical game (see "How to run the game")
- `AstroPartyAI-headless` — the no-window automated match runner (see
  "Headless execution")
- `tests/astroparty_tests` — the test suite (see "Testing instructions")

`CMakeLists.txt` locates SDL2 purely through `find_package(SDL2 REQUIRED)` —
it never hardcodes a Homebrew, vcpkg, or system path. If CMake cannot find
SDL2 automatically (most commonly on Apple Silicon with Homebrew, if your
shell doesn't already export `CMAKE_PREFIX_PATH`), point it at your SDL2
install explicitly:

```sh
cmake -B build -DCMAKE_PREFIX_PATH="$(brew --prefix sdl2)"
```

### Build options

| Option | Default | Effect |
|---|---|---|
| `ASTROPARTY_BUILD_TESTS` | `ON` | Builds the `astroparty_tests` suite and registers it with CTest |
| `ASTROPARTY_WARNINGS_AS_ERRORS` | `OFF` | Turns `-Wall -Wextra -Wpedantic` into hard errors |
| `ASTROPARTY_ENABLE_SANITIZERS` | `OFF` | Builds with AddressSanitizer + UndefinedBehaviorSanitizer |

### Building without network access

The build depends on `doctest` and `nlohmann/json`, both fetched
automatically via CMake's `FetchContent` the first time you configure with
tests enabled. If you need to configure offline, disable the test suite
(the main executables still need nlohmann/json, since `SimulationConfig`
and `MapDefinition` use it, so a fully offline build requires it to already
be present in your FetchContent cache or CMake package registry):

```sh
cmake -B build -DASTROPARTY_BUILD_TESTS=OFF
```

## How to run the game

```sh
./build/AstroPartyAI
```

Run it from the repository root, or from any working directory — the game
has no dependency on the current working directory (unlike the upstream
reference project, which required a path-relative `textures/` folder).

## Keyboard controls

| Player | Turn right | Shoot |
|---|---|---|
| Player 1 (blue) | `D` | `W` |
| Player 2 (red) | Right Arrow | Up Arrow |

| Action | Key |
|---|---|
| Restart the match | `R` |
| Quit | `Esc`, or close the window |

Both ships accelerate continuously and automatically — there is no "stop" or
"reverse" control, matching the original Astro Party mechanic: steering
(turning right) is your only way to avoid bullets and line up a shot.

The match outcome ("Player 1 wins!", "Draw!") and control reminder are shown
in the window's title bar.

## Headless execution

`AstroPartyAI-headless` plays one complete match with no SDL window, using
deterministic scripted controllers, and prints a JSON result:

```sh
./build/AstroPartyAI-headless --max-ticks 3600
```

```json
{
  "outcome": "ShipWon",
  "winningShipId": 0,
  "ticksElapsed": 212
}
```

`outcome` is one of `"ShipWon"`, `"Draw"` (every ship eliminated in the same
step), or `"Timeout"` (the match was still undecided when `--max-ticks` ran
out). Options:

| Flag | Default | Meaning |
|---|---|---|
| `--config <path>` | built-in Milestone-1-compatible defaults | A `SimulationConfig` JSON file (see "Configuration") |
| `--map <path>` | built-in two-ship corner layout | A `MapDefinition` JSON file (see "Map loading") |
| `--max-ticks <N>` | `3600` (60s at the default 60 Hz tick rate) | Fixed-step budget before declaring a timeout |
| `--output <path>` | stdout | Where to write the JSON result |

This binary links only the `astroparty_sim` library — never SDL2 — so it can
run in CI containers or any environment with no display attached, and can be
invoked repeatedly for batch evaluation scripts (a future Python harness
would shell out to it once per match).

## Controller architecture

`input::IShipController` is the single interface both human and scripted
controllers implement:

```cpp
class IShipController {
public:
    virtual core::ShipAction decide(const core::World& world, int controlledShipId) = 0;
};
```

- **`KeyboardShipController`** reads live SDL key state (used by the
  graphical game).
- **`ScriptedShipController`** replays a fixed, deterministic pattern with no
  reactive logic (used by the headless runner and tests) — it is
  intentionally *not* an AI.
- A future AI controller only needs to implement this same interface;
  `World::step()` and the headless runner never know or care which kind of
  controller produced an action.

Controllers receive `world` only by `const` reference, so there is no way
for a controller to mutate simulation state except through the `ShipAction`
it returns.

## Configuration

`core::SimulationConfig` holds every tunable simulation parameter (arena
size, tick rate, ship/bullet physics, collision radius, max match ticks).
Its defaults exactly reproduce Milestone 1's original hardcoded behavior.
Load one from JSON — any field it omits keeps its default:

```sh
./build/AstroPartyAI-headless --config configs/default_simulation.json
```

See `configs/default_simulation.json` for the full field list and
`include/astroparty/core/SimulationConfig.hpp` for per-field documentation.
`SimulationConfig::validate()` rejects non-positive sizes/speeds/rates and a
collision radius too large for the arena, each with a specific error message.

## Map loading

`core::MapDefinition` describes an arena's size, ship spawn points, and
obstacle placements, loadable from JSON:

```sh
./build/AstroPartyAI-headless --map configs/maps/default_arena.json
```

Two example maps are included: `configs/maps/default_arena.json` (the
Milestone 1 two-corner layout, now expressed as data) and
`configs/maps/example_with_obstacles.json` (adds obstacle entries).

**Obstacles are currently data only.** `MapDefinition` can describe and
validate an obstacle's position and radius, but `World` does not yet spawn a
corresponding entity or collide anything against it — that gameplay arrives
in Milestone 3 (see `astro-party-workspace/audit/06_implementation_roadmap.md`
and `docs/ARCHITECTURE.md`). Everything else a map describes (arena size,
ship spawn positions and headings) is fully functional today: `World`'s
default constructor itself is built on `MapDefinition::defaultTwoShipMap()`.

## Determinism

`World::step()` takes an explicit `deltaSeconds` and never reads the system
clock, a random number generator, or any other hidden input — its output is
a pure function of its current state and the actions passed in. There is
**no randomness anywhere in the simulation**, so no seed exists to manage:
identical config, map, and action sequence always produce an identical
result on the same platform. This is verified by automated tests that run
the same scripted match twice and compare both the summarized `MatchResult`
and the full final ship state (see `tests/test_headless_match_runner.cpp`).

**Limitation:** cross-platform bit-for-bit floating-point determinism (e.g.
identical results on ARM vs. x86) is not verified and not guaranteed —
determinism here means "repeatable on one platform/build," not "an
identical binary replay format."

## Current features (Milestone 2)

Everything from Milestone 1, plus:

- `input::IShipController` interface shared by human and scripted controllers
- `ScriptedShipController`: a deterministic, non-reactive controller for
  headless runs and tests
- `AstroPartyAI-headless`: a no-SDL executable that plays a full match and
  exports a structured JSON result (win / draw / timeout)
- `core::SimulationConfig`: externalized, validated, JSON-loadable
  simulation parameters (Milestone-1-compatible by default)
- `core::MapDefinition`: externalized, validated, JSON-loadable arena size
  and ship spawn points (obstacles are data-only for now)
- GitHub Actions CI building and testing on both Ubuntu and macOS on every
  push and pull request

## Planned AI extensions (not yet implemented)

These are explicitly **out of scope** for this milestone; they are listed
here so expectations are clear, and detailed further in
`astro-party-workspace/audit/06_implementation_roadmap.md`:

- Strategic, reactive AI controllers (Milestone 2 only ships a
  non-reactive scripted controller for testing)
- Team-based play, resource collection, and dynamic role assignment
  (Milestone 3)
- Functional obstacle collision gameplay (Milestone 3; currently data-only)
- Procedurally generated arenas with validity checks (Milestone 4)
- Python-based evaluation tooling and statistics
- Multiplayer networking (deliberately deferred; see `docs/ARCHITECTURE.md`)

## Testing instructions

```sh
cmake -B build
cmake --build build -j
ctest --test-dir build --output-on-failure
```

As of this milestone: **83 test cases, 1013 assertions**, covering vector
math, ship/bullet physics, collision resolution, match state, world
orchestration, controllers, headless match execution (win/draw/timeout),
determinism, configuration validation and loading, and map validation and
loading — all without creating an SDL window.

To build and run with sanitizers (recommended before trusting a change to
core simulation code):

```sh
cmake -B build-sanitized -DCMAKE_BUILD_TYPE=Debug -DASTROPARTY_ENABLE_SANITIZERS=ON
cmake --build build-sanitized -j
ctest --test-dir build-sanitized --output-on-failure
```

**Known environment limitation:** on macOS 26 ("Tahoe") with Apple Clang 17,
AddressSanitizer's own shadow-memory initialization hangs before `main()`
runs — confirmed with `sample` to be stuck inside
`__asan::InitializeShadowMemory()`, i.e. a toolchain/OS issue unrelated to
this project's code. `ASTROPARTY_ENABLE_SANITIZERS` enables both ASan and
UBSan together via `-fsanitize=address,undefined`; if it hangs on your
machine the same way, test with UBSan alone instead:

```sh
cmake -B build-ubsan -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_CXX_FLAGS="-fsanitize=undefined -fno-omit-frame-pointer" \
    -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=undefined"
cmake --build build-ubsan -j
ctest --test-dir build-ubsan --output-on-failure
```

All test cases pass cleanly under UBSan on macOS, and plainly on Ubuntu in
CI. Memory safety was additionally checked with macOS's `leaks` tool as an
ASan substitute (`leaks --atExit -- ./build/tests/astroparty_tests`), which
reported **0 leaks for 0 total leaked bytes**.

The simulation (`astroparty_sim`) has no SDL dependency, so the entire test
suite runs with no window, no display, and no input device required — which
is exactly what makes it CI-friendly.

## Continuous integration

Every push (to any branch, including `main`) and every pull request
targeting `main` runs `.github/workflows/ci.yml` ("Build & Test") on
GitHub-hosted Ubuntu and macOS runners: install SDL2 (apt / brew), configure
with CMake, build, and run the full CTest suite. The workflow also supports
manual triggering (`workflow_dispatch`).

Because this repository is private, the Actions badge at the top of this
file only renders for GitHub accounts with access to the repository — this
is standard GitHub behavior for private repos, not evidence the pipeline is
broken. Run `gh run list --repo FSChuang/AstroParty-AI` or open the
repository's Actions tab (with access) to see the actual run history.

## Team development workflow

- `main` is the stable branch. It should always build and pass its test
  suite.
- All work happens on a feature branch, named descriptively (e.g.
  `milestone-2/headless-runner`).
- Open a pull request targeting `main` for every change. Every PR must show
  a passing **Build & Test** check (both Ubuntu and macOS) before merging.
- Never force-push to `main`. Resolve merge conflicts on your feature branch
  before merging.
- Keep commits focused: separate infrastructure (CI, build config), gameplay
  code, and documentation changes where practical.

**Branch protection:** GitHub's branch protection rules and repository
rulesets both require a public repository or GitHub Pro on this account —
confirmed via the GitHub API (`403: Upgrade to GitHub Pro or make this
repository public to enable this feature`), not assumed. Until the team
upgrades or the repository goes public, the rules above are enforced by
convention and code review, not by GitHub itself: **do not merge a PR whose
Build & Test check has not passed**, regardless of merge permissions.

See `docs/ARCHITECTURE.md` for how the controller interface, headless
runner, configuration, and map loading fit together architecturally.
