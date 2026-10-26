# AstroParty-AI Architecture

This document explains how AstroParty-AI is organized and why, with a focus
on the requirement that has driven the design since Milestone 1: **the
simulation must be usable with no renderer, no input device, and no window
at all.** That requirement comes directly from the Phase 1 audit of the
reference project, [CAstroParty](https://github.com/kkysen/CAstroParty) (see
`docs/UPSTREAM_AUDIT.md`, item C-7), whose
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
                 core / entities / physics / ai / headless
                       (no SDL anywhere in here)
                               ▲
                               │
                  ┌────────────┴─────────────┐
                  │   AstroPartyAI-headless    │   src/headless_main.cpp
                  │ (headless entry point --   │   (links astroparty_sim only)
                  │  no window, no SDL at all) │
                  └────────────────────────────┘
```

- **`core` + `entities` + `physics` + `ai` + `headless`** (built as the
  `astroparty_sim` CMake target): the entire simulation, the rule-based AI,
  and everything needed to run either without a window. `World` owns a
  roster of `Ship`s, `Bullet`s, and (Team Resource Battle only) `Resource`s,
  steps them forward by a delta time, resolves collisions (including against
  `ObstacleDefinition`s), and tracks the match outcome in `MatchState` or
  `TeamMatchState` depending on `GameMode`. `SimulationConfig` and
  `MapDefinition` (both JSON-loadable) parameterize that roster, the arena,
  and the physics constants. `headless::runMatch`/`runTeamMatch` drive a
  `World` to completion using any `IShipController`s, including the `ai/`
  team controllers. **None of these headers or source files include any SDL
  header.** `astroparty_sim` links against the standard library and
  nlohmann/json only, which is what makes it buildable and testable in a
  plain unit test binary -- and linkable into a window-free executable.

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
  SDL's lifetime, the window, the `World`, and its controllers, and runs the
  main loop. `ApplicationOptions` (default: Classic Deathmatch) selects
  between two `KeyboardShipController` instances (Classic Deathmatch) or six
  `ai::TeamShipController` instances built by `ai::makeTeamControllers`
  (Team Resource Battle) -- the main loop, rendering calls, and restart
  handling are identical either way; only construction differs.

- **`src/headless_main.cpp`** is the headless executable's composition root:
  it parses a handful of CLI flags, optionally loads a `SimulationConfig`
  and `MapDefinition` from JSON, and dispatches on the resulting
  `World::mode()`: one `ScriptedShipController` per ship and
  `headless::runMatch` for Classic Deathmatch, or
  `ai::makeTeamControllers` and `headless::runTeamMatch` for Team Resource
  Battle, printing the resulting JSON result either way. It links
  `astroparty_sim` only.

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
| `ai::TeamShipController` | `astroparty_sim` | No | Delegates to a shared `ai::ITeamStrategy` for its ship's current goal, then to `ai::UnitBehaviors` to turn that goal into a `ShipAction`; reads `world` to steer and to let its strategy observe match state. See "AI module" below. |

`Application` and `headless::runMatch`/`headless::runTeamMatch` all treat
controllers polymorphically through this one interface -- adding the
rule-based AI controller above required no change to `World`, `Ship`,
`Bullet`, `MatchState`, or either match runner.

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
given by `--output`, so `scripts/run_team_experiments.py` (or any other
script) can invoke `AstroPartyAI-headless` once per match and parse its
JSON output.

`headless::runTeamMatch(World&, controllers, maxTicks)` is the Team
Resource Battle equivalent, identical in structure but driven by
`world.teamMatchState().hasEnded()` instead of `world.matchState()
.hasEnded()`. It returns a `TeamMatchResult`:

- `outcome`: `TeamWon`, `Draw`, or `Timeout` -- mirroring `MatchResult`'s
  split between a simulation-rule outcome (`core::TeamMatchOutcome`, which
  already resolves "the tick budget inside `SimulationConfig.maxMatchTicks`
  ran out" as a score-based `TeamWon`/`Draw`) and a *run-level* timeout
  (this call's own `maxTicks` argument being exhausted first, e.g. if it is
  set lower than the config's own `maxMatchTicks` for a quicker batch run).
- `winningTeamId`: set only when `outcome == TeamWon`.
- `ticksElapsed`, `teamScores`, `resourcesCollected`, `shipsDestroyed`: read
  directly from `World`'s own per-team accessors once the loop stops.

`src/headless_main.cpp` dispatches to `runMatch` or `runTeamMatch` based on
the constructed `World::mode()`, and builds controllers accordingly: one
`ScriptedShipController` per ship for Classic Deathmatch, or
`ai::makeTeamControllers(world, --team0-strategy, --team1-strategy)` for
Team Resource Battle.

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

**Functional vs. data-only, as of Milestone 3:**

| Map feature | Status |
|---|---|
| Arena size | Functional -- `World`'s bounds and clamping come directly from the map |
| Ship spawn points (position, heading, `teamId`) | Functional -- `World::spawnInitialShips()` reads them directly; `teamId` is used by Team Resource Battle and ignored (`-1`) by Classic Deathmatch |
| Obstacles | Functional -- `World` collides both ships and bullets against every `ObstacleDefinition` every step (see "Obstacles" below) |
| Home regions, resource spawn points | Functional, Team Resource Battle only (see "Game modes" below) |

Procedural map generation is Milestone 4 work. This milestone's obstacles
are still placed by hand in a static `MapDefinition`; nothing here assumes
or forecloses how a future generator would produce the same structure.

## Obstacles

`core::ObstacleDefinition` is just `{ position, radius }` -- a circle, the
simplest shape that needs no new collision primitive beyond the
circle-circle and point-circle tests `physics/Collision.hpp` already had for
ships and bullets. `physics/Obstacle.hpp` adds exactly two free functions:

- `circlesOverlap(centerA, radiusA, centerB, radiusB)` -- used both for
  ship/obstacle collision and, in `MapDefinition::validate()`, to reject a
  ship spawn point that overlaps an obstacle before the match ever starts.
- `pushOutOfCircle(position, radius, obstaclePosition, obstacleRadius)` --
  moves an overlapping circle to just outside the obstacle, along the line
  between the two centers (undefined only in the degenerate same-center
  case, which it resolves by not moving the circle at all).

This is a **deliberately approximate, non-physics-engine resolution**:
position-only correction, no velocity response, no restitution. A ship that
drives into an obstacle stops dead against it rather than bouncing; a bullet
that reaches one simply expires (`resolveBulletObstacleCollisions` in
`core/Collisions.hpp`). Both run every tick, in both game modes, so an
obstacle-filled map behaves identically whether it is being watched
graphically or run headless.

## Game modes

`core::GameMode` is a two-value enum (`ClassicDeathmatch`,
`TeamResourceBattle`) carried on `SimulationConfig`. Both modes share
**every** physics, movement, and collision code path in `World` --
`updateShips`, `updateBullets`, `resolveCollisions` (ships/bullets vs. each
other and vs. obstacles) run unconditionally regardless of `mode()`. The two
modes differ in exactly two places, both explicitly gated in `World::step()`:

1. **What happens to an eliminated ship.** Classic Deathmatch leaves it dead
   for the rest of the match (`MatchState` ends the match once one or zero
   ships remain). Team Resource Battle instead respawns it at its original
   map spawn point after `config().respawnDelayTicks` fixed steps
   (`World::updateRespawns()`).
2. **How the match's outcome is decided.** Classic Deathmatch uses
   `MatchState` (last-ship-standing / simultaneous-elimination draw).
   Team Resource Battle uses the separate `TeamMatchState` (below). Only the
   accessor matching the current `mode()` is meaningful; the other always
   reports `Undecided`.

`World::step()` gates `updateRespawns()` (and the resource pickup/delivery
step) behind `mode() == TeamResourceBattle` explicitly, rather than relying
on "the ship roster/resource list would just be empty in Classic
Deathmatch" -- an unconditional call would incorrectly revive a
permanently-eliminated Classic Deathmatch ship once `respawnDelayTicks`
elapsed. This is called out directly in `World.hpp`'s comments as a
correctness-critical detail, not a style preference.

### Team Resource Battle rules

A `World` constructed with `mode = TeamResourceBattle` requires its map to
provide, or its constructor throws `std::invalid_argument` immediately:
exactly three ship spawns on team 0 and three on team 1 (every spawn must
have `teamId` 0 or 1 -- no unassigned spawns are allowed in this mode), one
`HomeRegion` per team, and at least one resource spawn point.

The rules, chosen for simplicity over completeness:

- **Resource state.** A `Resource` (`entities::Resource`) is always in
  exactly one of two states: `AtSpawn` (sitting at its fixed spawn point,
  pickable) or `Carried` (owned by a specific ship, following that ship's
  position implicitly through the carrier's own `position()` -- a `Resource`
  has no independent position of its own while carried). There is
  deliberately no third "dropped at an arbitrary location" state, which
  would be a second, harder-to-keep-consistent source of truth for "where is
  this resource."
- **Pickup.** Any alive ship not already carrying a resource that comes
  within `config().resourcePickupRadius` of an `AtSpawn` resource picks it
  up. A ship can carry at most one resource at a time.
- **Delivery.** A carrying ship that reaches its *own* team's home region
  (within that region's radius) scores one point for its team and the
  resource immediately returns to `AtSpawn` at its original spawn point --
  delivering to the *other* team's home region does nothing.
- **Elimination while carrying.** A resource carried by a ship that is
  eliminated is immediately dropped back to `AtSpawn` (`World::
  dropResourcesCarriedByEliminatedShips()`, run before pickup/delivery each
  step) -- it does not sit at the elimination site, keeping "where can I
  find an available resource" a simple query over fixed spawn points.
- **Ship elimination/respawn.** An eliminated ship respawns at its original
  map spawn point and heading after `config().respawnDelayTicks` fixed
  steps; the match continues (no permanent elimination, unlike Classic
  Deathmatch).
- **Victory and timeout.** `TeamMatchState::evaluate()` is re-run every
  step: the first team to reach `config().winningScore` wins outright; if
  neither has, and `config().maxMatchTicks > 0` ticks have elapsed, whichever
  team has the higher score at that point wins, with equal scores called a
  `Draw` (`maxMatchTicks <= 0` means no tick-based resolution at all, so the
  match can only end by score).

`World` also tracks, per team, cumulative `resourcesCollected()` (every
pickup, whether or not later delivered) and `shipsDestroyed()` (every fresh
elimination, not re-incremented while a ship remains dead) -- both are
exposed for headless evaluation reporting (see below) independently of
`teamScores()` (delivered resources only).

## AI module (`ai/`)

Everything under `ai/` is part of `astroparty_sim` (no SDL dependency) and
is deterministic, rule-based logic -- no neural networks, no learned
policies, no Python at simulation time. It is organized in three layers,
each with a single responsibility:

```
TeamObservations (pure World queries)
        │
        ▼
ITeamStrategy::assignGoals()  →  vector<ShipGoal>     (team-level: "what should
  BaselineTeamStrategy                                  each ship be doing?")
  UtilityTeamStrategy
        │
        ▼
UnitBehaviors::moveToward() / pursueAndAttack()  →  ShipAction   (unit-level:
  (built on Steering.hpp primitives)                  "how do I actually do that,
        │                                              given my current position/
        ▼                                              heading/nearby obstacles?")
TeamShipController::decide()  →  implements IShipController
```

- **`TeamObservations.hpp`**: small, pure query functions over `const
  core::World&` (nearest available resource, nearest alive enemy, which
  enemy is carrying and closest to delivering, who on my team is carrying).
  Both strategies below call *only* these functions to read the match --
  neither has access to anything the other lacks, which is what makes a
  Strategic-vs-Baseline comparison fair (see "Baseline opponent" below).
- **`Steering.hpp` / `UnitBehaviors.hpp`**: `headingToward`,
  `clockwiseRotationNeeded`, `shouldTurnToFace`, and `isObstacleAhead` are
  the shared math every role is built from (the ship can only ever turn
  clockwise -- there is no turn-left control -- so steering is phrased as
  "how much clockwise rotation is needed," not a signed angle).
  `moveToward()` turns toward a target position, reactively turning away
  instead whenever `isObstacleAhead()` says the straight-ahead path is
  blocked (a simple reactive avoidance, not path planning -- acceptable at
  this project's scale of a handful of obstacles per map, not a dense
  field). `pursueAndAttack()` does the same steering and additionally fires
  once aimed and in range, independent of whether it is also dodging an
  obstacle that tick (bullet/obstacle collision is handled separately by
  `World`, so firing while turning is harmless, just sometimes wasted).
- **`TeamShipController`**: adapts an `ITeamStrategy` to `IShipController`.
  Multiple `TeamShipController`s on one team share one `ITeamStrategy`
  instance (`shared_ptr`) and each independently calls `assignGoals()` with
  identical inputs every tick; since `assignGoals()` is a pure function of
  `World`'s current state, this redundant recomputation (3 calls instead of
  1, for a 3-ship team) is simple, correct, and a non-issue at this
  project's scale. `decide()` looks up its own ship's goal, then dispatches
  `Interceptor`/`Escort` goals to `pursueAndAttack()` and
  `Collector`/`Defender` goals to `moveToward()`.

### Team-level strategic AI

`ITeamStrategy::assignGoals(world, teamId, teamShipIds)` returns one
`ShipGoal { role, targetPosition, targetShipId }` per ship, in order. This
is the **entire information flow** between the team-strategy layer and the
unit-controller layer: a strategy never produces a `ShipAction` itself
(it doesn't know how to steer), and a controller never decides roles itself
(it doesn't see the other ships' assignments) -- the two concerns are
strictly separated through this one data structure.

Suggested roles are `Collector`, `Defender`, `Interceptor`, `Escort`; not
every situation uses all four.

`UtilityTeamStrategy` is this project's real strategic AI: a simple,
explainable, **utility/priority-based** assignment, recomputed from scratch
every tick with no persisted "sticky" state, consuming ships from a shared
pool of this team's not-yet-assigned, alive ships in priority order:

1. **Collector (deliver).** Any ship already carrying a resource always
   heads home, overriding everything else.
2. **Interceptor.** If an enemy carrier is within a threat radius of this
   team's home region, the nearest remaining ship is sent to stop it.
3. **Escort.** If this team has a carrier and an enemy is closing in on it,
   the nearest remaining ship is sent to engage that threat.
4. **Collector (gather).** Each remaining ship is sent after the nearest
   currently-available resource.
5. **Defender.** Anyone still unassigned (e.g. no resource is currently
   available) patrols this team's home region.

Recomputing this fresh every tick, rather than maintaining explicit
"sticky" role state, is a deliberate choice: it is what makes **dynamic
reassignment** (a resource becoming available, an enemy carrier
approaching, a friendly collector being threatened, a ship being destroyed
or respawning) fall out automatically, with no separate event-detection
machinery -- a changed `World` state simply produces a different
`assignGoals()` result the next tick. `tests/test_team_strategies.cpp`
verifies this directly (a ship's goal visibly changing as it picks up and
then delivers a resource, a dead ship being excluded and silently absorbed
by its teammates, an Interceptor/Escort being assigned only once the
triggering condition is actually true).

### Baseline opponent

`BaselineTeamStrategy` is deliberately simpler, to give the strategic AI
something meaningful to be evaluated against:

- **Fixed roles**, never reassigned by match events: every ship but the
  last on the team is always a `Collector`; the last is always a
  `Defender`. There is no `Interceptor` or `Escort` role here at all.
- Collectors always greedily target the nearest available resource.
- The Defender patrols its own home region, engaging the nearest enemy only
  if one strays within a fixed radius of it -- reactive, not coordinated.
- A carrying ship still always overrides to heading home (the one piece of
  behavior every role must share, or it could never actually score).

Critically, `BaselineTeamStrategy` reads `World` through the exact same
`TeamObservations` queries `UtilityTeamStrategy` does -- neither has
privileged information (true match state, future knowledge, or a shortcut
past the actual `ShipAction`/movement constraints) the other lacks. The
headless runner and `scripts/run_team_experiments.py` can run any pairing
of the two (Strategic vs. Baseline, Baseline vs. Baseline, Strategic vs.
Strategic); Strategic vs. Baseline is the primary evaluation this project
makes, and no claim that one is "better" is made anywhere in this repo
without that comparison actually having been run (see `README.md`'s
headless section for how to run it yourself -- results are environment- and
map-dependent and are intentionally not hardcoded here).

### 3v3 controllers

Exactly like a human or scripted controller, a `TeamShipController` only
ever affects the simulation through the `ShipAction` it returns from
`decide()` -- it has no path to move a ship directly, teleport it, or read
anything `World`'s public `const` accessors don't already expose. Six
`TeamShipController` instances (three per team, two `ITeamStrategy`
instances shared within each team) are enough to run a full 3v3 match;
`ai::makeTeamControllers(world, team0StrategyName, team1StrategyName)`
builds exactly that wiring (grouping ships by `world.teamIdOfShip()`,
building each team's strategy via `ai::makeTeamStrategyByName`), and is
shared verbatim by both the headless executable and the graphical
`Application` so a 3v3 match is wired up identically regardless of which one
is observing it.

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
   `docs/UPSTREAM_AUDIT.md`, section 4). This
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
`docs/UPSTREAM_AUDIT.md`) so they read as
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
ship/bullet/obstacle collision rules, `MatchState`, `TeamMatchState`,
`Resource` and `World`'s Team Resource Battle orchestration (construction
validation, pickup/delivery/scoring/drop-on-elimination/respawn/reset,
cumulative metrics), `World`'s Classic Deathmatch orchestration (including
map-driven spawning), `SimulationConfig` and `MapDefinition`
validation/loading, `ScriptedShipController`, the `ai/` module (steering
primitives, `moveToward`/`pursueAndAttack`, both team strategies' role
assignment and dynamic reassignment, `TeamShipController`), and both
`headless::runMatch` and `headless::runTeamMatch` (win, draw, timeout,
determinism, and result serialization) -- all without creating a window.
See `README.md` for exact build and test commands, current test/assertion
counts, and sanitizer usage.

## Continuous integration

`.github/workflows/ci.yml` ("Build & Test") runs on every push and every
pull request targeting `main`, on both `ubuntu-latest` and `macos-latest`
GitHub-hosted runners: install SDL2 (apt / brew), `cmake -S . -B build`,
`cmake --build build --parallel`, `ctest --test-dir build
--output-on-failure`. It uses the repository's default (read-only)
`GITHUB_TOKEN` permissions and installs no external actions beyond
`actions/checkout`. `main` is a protected branch: both CI jobs must pass
and at least one approving review is required before a pull request can
merge, and force-pushes/deletions of `main` are disabled at the repository
level (verified via the GitHub API, not assumed). See `CONTRIBUTING.md` for
the contribution workflow this enforces.
