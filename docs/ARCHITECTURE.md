# AstroParty-AI Architecture

This document explains how AstroParty-AI is organized and why, with a focus
on the one requirement that drove most of the design: **the simulation must
be usable with no renderer, no input device, and no window at all.** That
requirement comes directly from the Phase 1 audit of the reference project,
[CAstroParty](https://github.com/kkysen/CAstroParty) (see
`astro-party-workspace/audit/04_issues_and_risks.md`, item C-7), whose
`Player`/`Bullet` structs embedded an `SDL_Texture*` directly and whose input
handling wrote straight into the simulation's global state. AstroParty-AI
exists to make that mistake structurally impossible to repeat, so that a
future milestone can run thousands of AI-vs-AI matches with no window open
at all.

## Layers

```
            ┌───────────────────────────────┐
            │          Application          │   src/Application.{hpp,cpp}
            │  (the only class that knows    │
            │   about every other layer)     │
            └───────────────┬────────────────┘
                 ┌───────────┼────────────┐
                 ▼           ▼            ▼
          ┌───────────┐ ┌─────────┐ ┌───────────┐
          │   input   │ │   core   │ │ rendering │
          │ (SDL keys)│ │(World,   │ │ (SDL draw)│
          │           │ │ Ship,    │ │           │
          │           │ │ Bullet)  │ │           │
          └─────┬─────┘ └────┬─────┘ └─────┬─────┘
                │             │              │
                └─────────────┼──────────────┘
                               ▼
                         (no SDL here)
                    core / entities / physics
```

- **`core` + `entities` + `physics`** (built as the `astroparty_sim` CMake
  target): the entire simulation. `World` owns a fixed roster of `Ship`s and
  a dynamic list of `Bullet`s, steps them forward by a delta time, resolves
  collisions, and tracks the match outcome in `MatchState`. **None of these
  headers or source files include any SDL header.** `astroparty_sim` links
  against nothing but the standard library, which is what makes it buildable
  and testable in a plain unit test binary.

- **`input`**: translates a live input source into a `core::ShipAction`
  (`{ bool turnRight; bool shoot; }`). The only implementation today,
  `KeyboardShipController`, reads SDL's keyboard state. It implements
  `IShipController`, the seam a future AI controller will also implement —
  see "Where AI plugs in" below.

- **`rendering`**: draws a `const core::World&` using SDL primitives.
  `SdlLibraryGuard`, `SdlWindow`, and `SdlRenderer` are thin RAII wrappers
  that own SDL's init/window/renderer lifetimes; `GameRenderer` is the class
  that actually looks at ships and bullets and draws them. Rendering only
  ever *reads* `World` through its const accessors.

- **`Application`** (in `src/`, not `include/`, since it is the executable's
  composition root rather than a reusable library type): owns SDL's
  lifetime, the window, the `World`, and the two `KeyboardShipController`
  instances, and runs the main loop. It is deliberately the only class that
  depends on all three other layers at once.

## Why a fixed timestep

`Application::run()` measures real elapsed wall-clock time with
`std::chrono::steady_clock` and accumulates it; `World::step()` is then
called repeatedly with a **fixed** `deltaSeconds` of `1/60` second until the
accumulator drops below that threshold. Rendering happens once per loop
iteration, independent of how many (zero, one, or several) fixed steps just
ran.

This matters for two reasons:

1. **It decouples simulation rate from display rate.** A fast or slow
   monitor does not change gameplay — the same input sequence always
   produces the same sequence of `World` states. This is a prerequisite for
   deterministic batch evaluation in a later milestone
   (`astro-party-workspace/audit/06_implementation_roadmap.md`, Milestone 2).
2. **It preserves upstream's game feel exactly.** Upstream CAstroParty
   applied its movement constants once per tick at an implicit, uncapped 60
   Hz rate with no delta-time scaling at all (see
   `astro-party-workspace/audit/03_code_architecture.md`, section 4). This
   project expresses the same constants in per-second units (see
   `include/astroparty/physics/PhysicsConstants.hpp` for the full derivation)
   and multiplies by `deltaSeconds`; at exactly `1/60` s per step, the
   numeric effect is identical to upstream.

## Where AI plugs in

`World::step()` takes a `std::vector<core::ShipAction>`, one entry per ship,
and does not know or care where each entry came from. Today, `Application`
collects those actions by calling `pollAction()` on two
`KeyboardShipController` instances. A future `AIController` only needs to
implement `input::IShipController` — i.e. produce a `ShipAction` — to control
a ship; no change to `World`, `Ship`, `Bullet`, or `MatchState` is required.
This is the direct implementation of the "human and AI controllers share one
action interface" requirement from the project's course deliverables.

Two further properties make later AI work tractable:

- **Headless-readiness.** Nothing in `astroparty_sim` touches SDL, so a
  future headless batch-evaluation binary can link `astroparty_sim` alone,
  drive `World::step()` from any number of `IShipController`s (human or AI),
  and never construct a window.
- **Inspectable state.** `World` exposes `ships()`, `bullets()`, and
  `matchState()` as plain const references over simple value types, with no
  SDL or rendering handles mixed in — this is what a future AI controller or
  Python evaluation harness would read to make decisions or compute
  statistics.

## Deliberate differences from upstream CAstroParty

These are documented here (and in more detail in
`astro-party-workspace/audit/05_cpp_migration_plan.md`) so they read as
decisions, not oversights:

| Area | Upstream | AstroParty-AI | Why |
|---|---|---|---|
| Controls | Mouse clicks, one local player | Keyboard, two local players (P1: D/W, P2: Arrow Right/Up) | Milestone 1 requires two simultaneous local players; a mouse cannot drive two players independently. |
| Match end | Window-title-less; a 60-frame `you_win_timer` with an acknowledged `-99999` hack auto-restarted the match | `MatchState` with an explicit `Undecided / ShipWon / Draw` outcome; window title reports it; `R` restarts manually | Upstream's own author flagged the timer hack as unclean; this project also adds an explicit draw outcome upstream never had. |
| Rendering | Textured sprites (`SDL_Texture`) embedded in `Player`/`Bullet` | Vector primitives (lines, filled rects); no texture assets | Phase 2 instructions explicitly deprioritize art for Milestone 1; this also removes the sprite-texture dependency that caused upstream's simulation/rendering coupling. |
| Collision/clamp radius | Derived from loaded sprite pixel dimensions | Fixed constant `kShipCollisionRadius` tuned to the drawn primitive | There is no texture to measure; see `PhysicsConstants.hpp` for the explicit note that this value is not a verified port of any upstream number. |
| Ship roster | Dynamic array, capacity 1000, add/remove at runtime | Fixed at exactly 2 ships for the whole match | Matches Milestone 1's actual scope; avoids upstream's index-invalidation and swap-remove complexity for a feature (large dynamic rosters) this milestone does not need. |
| Networking | Raw TCP, non-authoritative relay server | Not implemented | Explicitly out of scope per the Phase 2 instructions and the audit's risk assessment (`04_issues_and_risks.md`, item C-8). |

## Testing strategy

`astroparty_sim` has no SDL dependency, so `tests/` links against it alone
(see `tests/CMakeLists.txt`) and exercises `Vector2`, `Ship`, `Bullet`, the
`resolveShipBulletCollisions` collision rule, `MatchState`, and `World`'s
orchestration — all without creating a window. See `README.md` for exact
build and run commands.
