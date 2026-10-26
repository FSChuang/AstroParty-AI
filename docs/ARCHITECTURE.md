# AstroParty-AI Architecture

This document explains how AstroParty-AI is organized and why, with a focus
on the requirement that has driven the design since Milestone 1: **the
simulation must be usable with no renderer, no input device, and no window
at all.** That requirement comes directly from the Phase 1 audit of the
reference project, [CAstroParty](https://github.com/kkysen/CAstroParty) (see
`astro-party-workspace/audit/04_issues_and_risks.md`, item C-7), whose
`Player`/`Bullet` structs embedded an `SDL_Texture*` directly and whose input
handling wrote straight into the simulation's global state. Milestone 2 is
the first payoff of that decision: a headless match runner that links none
of the SDL-dependent code at all.

## Layers

```
            ┌───────────────────────────────┐
            │          Application          │   src/Application.{hpp,cpp}
            │  (the only class that knows    │   (graphical entry point)
            │   about rendering AND input)   │
            └───────────────┬────────────────┘
                 ┌───────────┼────────────┐
                 ▼           ▼            ▼
          ┌───────────┐ ┌─────────┐ ┌───────────┐
          │   input   │ │   core   │ │ rendering │
          │ (SDL keys)│ │(World,   │ │ (SDL draw)│
          │           │ │ config,  │ │           │
          │           │ │ map)     │ │           │
          └─────┬─────┘ └────┬─────┘ └─────┬─────┘
                │             │              │
                └─────────────┼──────────────┘
                               ▼
                    core / entities / physics / headless
                       (no SDL anywhere in here)
                               ▲
                               │
                  ┌────────────┴─────────────┐
                  │   AstroPartyAI-headless    │   src/headless_main.cpp
                  │ (headless entry point --   │   (links astroparty_sim only)
                  │  no window, no SDL at all) │
                  └────────────────────────────┘
```

- **`core` + `entities` + `physics` + `headless`** (built as the
  `astroparty_sim` CMake target): the entire simulation, plus everything
  needed to run it without a window. `World` owns a roster of `Ship`s and a
  dynamic list of `Bullet`s, steps them forward by a delta time, resolves
  collisions, and tracks the match outcome in `MatchState`. `SimulationConfig`
  and `MapDefinition` (both JSON-loadable) parameterize that roster and the
  physics constants. `headless::runMatch` drives a `World` to completion
  using any `IShipController`s. **None of these headers or source files
  include any SDL header.** `astroparty_sim` links against the standard
  library and nlohmann/json only, which is what makes it buildable and
  testable in a plain unit test binary -- and linkable into a window-free
  executable.

- **`input`**: produces a `core::ShipAction` from some decision source, via
  the shared `IShipController` interface (see "Controller architecture"
  below). `KeyboardShipController` (SDL-dependent) and `ScriptedShipController`
  (not SDL-dependent) both implement it.

- **`rendering`**: draws a `const core::World&` using SDL primitives.
  `SdlLibraryGuard`, `SdlWindow`, and `SdlRenderer` are thin RAII wrappers
  that own SDL's init/window/renderer lifetimes; `GameRenderer` is the class
  that actually looks at ships and bullets and draws them, sizing the drawn
  ship triangle from `world.config().shipCollisionRadius` so the silhouette
  always matches what can actually be hit. Rendering only ever *reads*
  `World` through its const accessors.

- **`Application`** (in `src/`, not `include/`, since it is the graphical
  executable's composition root rather than a reusable library type): owns
  SDL's lifetime, the window, the `World`, and the two `KeyboardShipController`
  instances, and runs the main loop.

- **`src/headless_main.cpp`** is the headless executable's composition root:
  it parses a handful of CLI flags, optionally loads a `SimulationConfig`
  and `MapDefinition` from JSON, builds one `ScriptedShipController` per
  ship, calls `headless::runMatch`, and prints the resulting `MatchResult` as
  JSON. It links `astroparty_sim` only.

## Controller architecture

`IShipController` is the single interface every ship-controlling source
implements, human or otherwise:

```cpp
class IShipController {
public:
    virtual core::ShipAction decide(const core::World& world, int controlledShipId) = 0;
};
```

- `world` is passed by `const` reference: an implementation can read any
  ship's position, velocity, or the match outcome to inform its decision,
  but has no path to mutate simulation state except through the
  `ShipAction` it returns. This is enforced by the type system, not by
  convention.
- `decide()` is not `const` on the controller object itself, so an
  implementation may advance its own internal state between calls (a tick
  counter, a target lock, …) even though it must never touch `world`.
- `controlledShipId` tells a controller which entry in `world.ships()` it is
  responsible for, so the same controller type can drive any ship.

Two implementations exist today:

| Controller | Lives in | SDL? | Behavior |
|---|---|---|---|
| `KeyboardShipController` | `astroparty_platform` | Yes | Reads live SDL key state for two fixed scancodes; ignores `world` entirely. |
| `ScriptedShipController` | `astroparty_sim` | No | Replays a fixed, repeating pattern (turn on alternating ticks; shoot every N ticks); also ignores `world`. **Not an AI** -- it exists so the headless runner and tests have a controller that needs no human and no reactive logic. |

A future AI controller only needs to implement `IShipController` -- producing
a `ShipAction` from whatever logic it likes, optionally reading `world` -- to
control a ship. Neither `World`, `Ship`, `Bullet`, nor `MatchState` changes.
`Application` and `headless::runMatch` both already treat controllers
polymorphically through this one interface, so adding an AI controller is
purely additive.

## Headless execution

`headless::runMatch(World& world, controllers, maxTicks)` is the one
function that plays a complete match with no rendering: once per fixed
timestep (`world.config().fixedDeltaSeconds()`), it asks every controller to
`decide()`, steps `world` with the resulting actions, and stops when either
the match ends or `maxTicks` fixed steps have run. It returns a `MatchResult`
with:

- `outcome`: `ShipWon`, `Draw` (every ship eliminated in the same step), or
  `Timeout` (the tick budget ran out while the match was still undecided).
  `Timeout` is deliberately a property of `headless::MatchResult`, not of
  `core::MatchState` -- it describes the *run*, not a simulation rule, so it
  does not belong in the core simulation type that the graphical game also
  uses (which has no concept of a tick budget at all).
- `winningShipId`: set only when `outcome == ShipWon`.
- `ticksElapsed`: how many fixed steps actually ran.

`MatchResult::toJsonString()` serializes this with nlohmann/json. The
headless executable either prints it to stdout or writes it to the path
given by `--output`, so a future batch-evaluation script (Python or
otherwise) can invoke `AstroPartyAI-headless` once per match and parse its
JSON output.

## Configuration and map loading

`core::SimulationConfig` is a flat, JSON-loadable struct holding every
tunable simulation parameter (arena size, tick rate, ship/bullet physics,
collision radius, max match ticks). Its default member initializers
reproduce Milestone 1's original hardcoded behavior exactly -- the
derivation from upstream CAstroParty's per-tick constants is documented
inline in `include/astroparty/core/SimulationConfig.hpp`.
`SimulationConfig::validate()` rejects physically nonsensical values (a
non-positive size, speed, or rate; a collision radius too large for the
arena) with a specific message naming the offending field, both when
constructed directly and when loaded from a file.

`core::MapDefinition` describes an arena's size, a list of ship spawn
points (position + heading), and a list of obstacle placements (position +
radius), also JSON-loadable and validated (at least two spawn points, every
spawn point inside the arena, every obstacle radius positive).

**Functional vs. data-only, as of Milestone 2:**

| Map feature | Status |
|---|---|
| Arena size | Functional -- `World`'s bounds and clamping come directly from the map |
| Ship spawn points (position, heading) | Functional -- `World::spawnInitialShips()` reads them directly, replacing Milestone 1's hardcoded two corners |
| Obstacles | **Data only.** `MapDefinition` parses and validates them; `World` does not spawn a corresponding entity or collide anything against one yet. |

Obstacle gameplay, 3v3 teams, and resource collection are Milestone 3 work
(`astro-party-workspace/audit/06_implementation_roadmap.md`); procedural map
generation is Milestone 4. This milestone's job was only to make sure a map
*can* describe obstacles and that `World` *can* be built from external data
at all -- both now true, with the actual obstacle interaction left for the
milestone that needs it.

`World` has two constructors: the default one (`World()`), which uses
`SimulationConfig{}` and `MapDefinition::defaultTwoShipMap()` and is exactly
Milestone 1's behavior (existing tests and the graphical game both still use
it unchanged), and `World(SimulationConfig, MapDefinition)` for everything
else. A map's `arenaSize` always wins over the config's, so the two can never
disagree about the playfield bounds.

## Why a fixed timestep

`Application::run()` measures real elapsed wall-clock time with
`std::chrono::steady_clock` and accumulates it; `World::step()` is then
called repeatedly with a **fixed** `deltaSeconds` (`config.fixedDeltaSeconds()`,
`1/60` s by default) until the accumulator drops below that threshold.
Rendering happens once per loop iteration, independent of how many (zero,
one, or several) fixed steps just ran. `headless::runMatch` uses the same
fixed `deltaSeconds` with no wall-clock measurement at all -- it just calls
`world.step()` in a tight loop, which is what makes a headless run both fast
and deterministic.

This matters for two reasons:

1. **It decouples simulation rate from display rate (or from real time
   entirely, in the headless case).** The same input/action sequence always
   produces the same sequence of `World` states.
2. **It preserves upstream's game feel exactly.** Upstream CAstroParty
   applied its movement constants once per tick at an implicit, uncapped 60
   Hz rate with no delta-time scaling at all (see
   `astro-party-workspace/audit/03_code_architecture.md`, section 4). This
   project expresses the same constants in per-second units and multiplies
   by `deltaSeconds`; at exactly `1/60` s per step (`SimulationConfig`'s
   default `tickRateHz`), the numeric effect is identical to upstream.

## Determinism

`World::step()`'s only inputs are its current state, the `ShipAction`s
passed in, and `deltaSeconds` -- it never reads the system clock or any
other hidden input. **There is no random number generator anywhere in the
simulation**, so there is no seed to manage: nothing here is probabilistic.
Combined with the fixed timestep above, this means identical config, map,
and action sequence always produce an identical result on the same platform
and build. `tests/test_headless_match_runner.cpp` verifies this directly by
running the same scripted match twice and comparing both the summarized
`MatchResult` and the full final ship positions/velocities.

**Limitation:** this is "repeatable on one platform/build," not "an
identical binary replay format" -- bit-for-bit identical floating-point
results across different compilers, optimization levels, or CPU
architectures (e.g. x86 vs. ARM) are not verified and are not a goal of this
milestone.

## Deliberate differences from upstream CAstroParty

These are documented here (and in more detail in
`astro-party-workspace/audit/05_cpp_migration_plan.md`) so they read as
decisions, not oversights:

| Area | Upstream | AstroParty-AI | Why |
|---|---|---|---|
| Controls | Mouse clicks, one local player | Keyboard, two local players (P1: D/W, P2: Arrow Right/Up) | Milestone 1 requires two simultaneous local players; a mouse cannot drive two players independently. |
| Match end | Window-title-less; a 60-frame `you_win_timer` with an acknowledged `-99999` hack auto-restarted the match | `MatchState` with an explicit `Undecided / ShipWon / Draw` outcome; window title reports it; `R` restarts manually | Upstream's own author flagged the timer hack as unclean; this project also adds an explicit draw outcome upstream never had. |
| Rendering | Textured sprites (`SDL_Texture`) embedded in `Player`/`Bullet` | Vector primitives (lines, filled rects); no texture assets | Phase 2 instructions explicitly deprioritize art for Milestone 1; this also removes the sprite-texture dependency that caused upstream's simulation/rendering coupling. |
| Gameplay constants | Hardcoded per-tick constants in C source | `SimulationConfig`, JSON-loadable, with Milestone-1-identical defaults | Milestone 2 requires configurable, validated parameters for future batch experiments. |
| Ship roster / spawn points | Dynamic array, capacity 1000; spawn points hardcoded in C source | Roster size and spawn points come from a `MapDefinition`; defaults to Milestone 1's two-corner layout | Matches each milestone's actual scope while making external map data possible; avoids upstream's index-invalidation and swap-remove complexity for a feature (large dynamic rosters) no current milestone needs. |
| Networking | Raw TCP, non-authoritative relay server | Not implemented | Explicitly out of scope per the Phase 2 instructions and the audit's risk assessment (`04_issues_and_risks.md`, item C-8). |

## Testing strategy

`astroparty_sim` has no SDL dependency, so `tests/` links against it alone
(see `tests/CMakeLists.txt`) and exercises `Vector2`, `Ship`, `Bullet`, the
`resolveShipBulletCollisions` collision rule, `MatchState`, `World`'s
orchestration (including map-driven spawning), `SimulationConfig` and
`MapDefinition` validation/loading, `ScriptedShipController`, and
`headless::runMatch` (win, draw, timeout, determinism, and `MatchResult`
serialization) -- all without creating a window. See `README.md` for exact
build and test commands, current test/assertion counts, and sanitizer usage.

## Continuous integration

`.github/workflows/ci.yml` ("Build & Test") runs on every push and every
pull request targeting `main`, on both `ubuntu-latest` and `macos-latest`
GitHub-hosted runners: install SDL2 (apt / brew), `cmake -S . -B build`,
`cmake --build build --parallel`, `ctest --test-dir build
--output-on-failure`. It uses the repository's default (read-only)
`GITHUB_TOKEN` permissions and installs no external actions beyond
`actions/checkout`. See `README.md`'s "Continuous integration" and "Team
development workflow" sections for how this fits into the group's merge
process, and for the documented limitation around branch protection on a
private repository under the current GitHub plan.
