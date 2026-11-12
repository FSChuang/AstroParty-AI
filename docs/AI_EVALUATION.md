# AI Evaluation and Improvement (Milestone 5)

This document covers the Milestone 5 work: an audit of the existing AI
system, measured diagnostics of its actual behavior, two new team
strategies compared against the existing ones, and a 360-match study across
60 procedurally generated maps (Milestone 4). It assumes familiarity with
`docs/ARCHITECTURE.md`'s "AI module" section and does not repeat it.

## 1. The AI system, reviewed layer by layer

The existing AI (unchanged by this milestone except where noted) splits
cleanly into four layers, each with one job:

| Layer | Question it answers | Where |
|---|---|---|
| **A. Strategic** | Which ship performs which task? | `ai::ITeamStrategy::assignGoals()` (`BaselineTeamStrategy`, `UtilityTeamStrategy`, and this milestone's `FixedRoleTeamStrategy`/`HysteresisTeamStrategy`) |
| **B. Tactical** | Which target should a ship pursue? | Folded into each strategy's `assignGoals()`: a `ShipGoal`'s `targetPosition`/`targetShipId` *is* the tactical target choice (nearest resource, nearest threatening enemy carrier, ...) |
| **C. Motion control** | How does a ship physically reach its target? | `ai::Steering` + `ai::UnitBehaviors` (`moveToward`, `pursueAndAttack`) -- shared by every strategy |
| **D. Combat control** | How does a ship aim, shoot, and survive? | `pursueAndAttack`'s aim/range check (layer C); survival is entirely physics (`core::Collisions`) -- there is no separate "combat AI," no dodging, no health |

`ai::TeamShipController` is the adapter between A/B (a `ShipGoal`) and C/D
(an actual `ShipAction`) -- see its own doc comment for the exact dispatch.
The Milestone 4 instruction not to assume poor performance comes from the
strategic layer was taken literally: **diagnostics were built and run
before any strategy change was designed** (Section 2 below).

## 2. Diagnostics: measurable AI behavior (`diagnostics/MatchDiagnostics.hpp`)

Metrics, precisely defined (see the header's own doc comments for the
exact semantics), computed by `diagnostics::runInstrumentedTeamMatch` --
a drop-in instrumented replacement for `headless::runTeamMatch`:

- **Low-progress ticks**, not low velocity. A ship accelerates every tick
  it is alive (there is no brake), so instantaneous speed is never a
  useful "is this ship stuck" signal -- it is near zero only right after a
  respawn. Instead, a ship is flagged low-progress on any tick where its
  **net straight-line displacement over a trailing 120-tick (2 s) window**
  stays below 50 units, regardless of how fast or erratically it moved
  within that window.
- **Shots fired**: edge-triggered (`ShipAction::shoot` transitioning
  false->true), i.e. actual bullets, not ticks spent holding the button.
- **Obstacle collision events**: edge-triggered contact with an obstacle.
  This project's physics has no ship-vs-ship collision at all, so this is
  the only kind of solid-geometry contact a ship can have.
- **Role changes**: ticks where a team strategy's returned `ShipRole` for
  a ship differs from the previous tick's, observed via a read-only query
  to the team's own `ITeamStrategy` (the exact same object driving play).
- **Delivery latency**: ticks between a resource being picked up and that
  team's score increasing from delivering it (not merely dropped on
  elimination) -- one sample per successful delivery.
- **Successful hits**: in this project's one-hit-kill combat model (see
  `core::Collisions::resolveShipBulletCollisions`), "a bullet landed" and
  "a ship was eliminated" are the same event -- `World::shipsDestroyed()`
  (already reported by `headless::TeamMatchResult`) *is* this count; it is
  not duplicated here.

### Findings from real matches (before any AI change)

Running `strategic` vs. `baseline` on the existing fixed `team_arena.json`
map and on the three procedurally generated families (`configs/procgen/examples/`)
with `AstroPartyAI-headless --diagnostics`:

- **13-48% of ship-ticks were flagged low-progress**, varying by map and
  matchup (lowest on the chokepoint example, highest on clustered).
- **Resource delivery latency ranged roughly 700-2000 ticks (12-33 s)**
  per successful delivery, out of a 3600-tick (60 s) match.
- **Combat was rare**: most matches recorded 0-6 total shots fired across
  all six ships for the entire match. Where both teams ran `strategic`
  (which alone has Interceptor/Escort), a custom symmetric test map
  produced a heavier-combat stalemate where carriers were repeatedly
  eliminated before ever reaching home (`shipsDestroyed` 1:3 over one
  match) -- a genuine, map-dependent emergent dynamic, not a bug; see
  `tests/test_match_diagnostics.cpp`'s scenario 3 comment for where this
  was caught and worked around in testing.

### Isolated motion-control scenario (Section 3's scenarios 1 and 2)

To separate "is the motion controller capable of carrying out an assigned
goal" from strategic-layer effects, `ai::moveToward` was driven directly
(no `World`, no strategy) toward a **static** target:

- **Open arena, no obstacle**: the ship *does* reach within 30 units of
  the target (confirming it is reachable at all), but then overshoots --
  continuous acceleration means it can never stop, and clockwise-only
  turning means "coming back" requires a wide loop, not a reversal. Across
  a 3600-tick run, it spent only ~15% of ticks within 60 units of the
  target after that first approach.
- **A single obstacle directly on the line to the target**: the ship still
  eventually passes near the target (closest approach ~65 units), routing
  around the obstacle without ever settling inside it, but the same
  overshoot-and-loop pattern dominates afterward.

**This is a genuine, measured limitation of `moveToward`'s steering**, not
assumed. An experimental "arrival + tangential orbit" steering variant
(steer at a point offset from the target, inside an arrival radius, to
encourage circling instead of a wide flyby) was implemented and tuned
across a grid of arrival-radius/orbit-radius combinations (40-120 units /
15-45 units) -- **none exceeded ~3% time-on-target**, no better than the
unmodified baseline's post-overshoot behavior. **This was not shipped.**
`ai::moveToward`/`pursueAndAttack` are unchanged by this milestone,
preserved exactly as the baseline motion controller, per the instruction
to only modify motion control when a fix is actually validated and to
report unsuccessful attempts honestly rather than ship an unvalidated
change.

In real gameplay this matters less than the isolated scenario suggests,
because `ShipGoal` targets change dynamically (pickup -> deliver
immediately redirects the ship), so "holding station" is rarely required
-- but it is a real, if partial, contributor to the measured delivery
latencies above, and is recorded here as a known limitation (see
"Known limitations" below) rather than silently worked around.

## 3. Strategy variants

All four strategies implement the identical `ai::ITeamStrategy` interface
and are selectable by name via `ai::makeTeamStrategyByName()` (and
therefore via `--team0-strategy`/`--team1-strategy` on both
`AstroPartyAI-headless` and `AstroPartyAI --mode team`):

| Name | Class | Status |
|---|---|---|
| `baseline` | `BaselineTeamStrategy` | Unchanged since Milestone 3 |
| `strategic` | `UtilityTeamStrategy` | Unchanged since Milestone 3 |
| `fixed` | `FixedRoleTeamStrategy` | **New.** Variant A |
| `hysteresis` | `HysteresisTeamStrategy` | **New.** Variant C |

### Variant A: `FixedRoleTeamStrategy` ("fixed")

A literally fixed assignment by ship position -- position 0 = Collector,
1 = Defender, 2 = Interceptor -- never reassigned for strategic reasons.
The only exceptions are the ones every strategy in this project already
makes for events the game's own rules force a response to (an eliminated
ship, a ship currently carrying a resource), never a judgment call. See
the class's own doc comment for exactly how this differs from the
pre-existing `BaselineTeamStrategy` (which already has fixed roles by
index, but its Defender still reactively engages a nearby enemy).

### Variant B: `UtilityTeamStrategy` ("strategic")

Unmodified. See `docs/ARCHITECTURE.md`.

### Variant C: `HysteresisTeamStrategy` ("hysteresis")

Chosen over the other candidates (global-utility-maximizing assignment,
which would need an assignment-problem solver disproportionate to a 3-ship
team; role commitment was the diagnostics-justified, simplest fix) because
the diagnostic data showed `UtilityTeamStrategy` recomputing its hard
distance thresholds (`kInterceptRadius`, `kEscortThreatRadius`) from
scratch every tick with no memory -- a textbook setup for threshold
oscillation when a threat hovers near the boundary.

- **Inputs**: the identical `ai::TeamObservations` queries every strategy
  in this project uses -- no privileged information.
- **Scoring/assignment objective**: `UtilityTeamStrategy`'s own priority
  order, unchanged (carrying -> deliver; intercept; escort; gather
  nearest; patrol).
- **Role selection**: identical to `UtilityTeamStrategy` for any ship with
  no active commitment.
- **Reassignment conditions (the actual change)**: once assigned
  Interceptor/Escort, a ship **commits** to that specific role and target
  for a minimum of 90 ticks (1.5 s), and after that floor, only drops the
  commitment once the target is no longer a valid threat at all (dead, or
  no longer carrying) or has cleared a radius **1.5x wider** than the one
  that triggered the commitment. Both halves -- the time floor and the
  wider exit radius -- are the hysteresis band.
- **Computational cost**: `O(teamShipIds.size())` per call, identical
  order to `UtilityTeamStrategy`, plus `O(1)` hash-map bookkeeping per
  ship.
- This is the project's **first stateful** team strategy. Since every
  `ai::TeamShipController` on a team calls `assignGoals()` independently
  with identical inputs (up to 3x per real tick), `assignGoals()` caches
  its result keyed on `(teamId, world.elapsedTicks())` so commitment
  timers advance exactly once per real tick -- see
  `tests/test_hysteresis_strategy.cpp`'s cache-collapsing test.

## 4. Experimental design

`scripts/run_strategy_comparison.py` (configuration:
`experiments/configs/strategy_comparison.json`) is the single reproducible
entry point:

```sh
cmake --build build -j
python3 scripts/run_strategy_comparison.py \
  --mapgen-executable build/AstroPartyAI-mapgen \
  --headless-executable build/AstroPartyAI-headless
```

- Generates **20 maps per family** (open/clustered/chokepoint; seeds
  1000-1019; 60 maps total) with `AstroPartyAI-mapgen`, reusing each
  map's own procgen geometry metrics from its manifest.
- For every map, runs every pairing of `{fixed, strategic, hysteresis}`
  with **both side assignments** (6 matches per map, 360 total), via
  `AstroPartyAI-headless --diagnostics`, recording map identity, seed,
  full `TeamMatchResult`, and full `MatchDiagnostics`.
- **Maps, not matches, are the independent sampling unit.** The two side-
  swapped matches on one map are a paired observation (same geometry, same
  deterministic initial state, no stochastic component anywhere in this
  project to seed or control beyond the map's own seed) -- never counted
  as two independent trials. The pairwise comparison computes, per map,
  strategy A's win rate across its two sides minus strategy B's, and
  reports a 95% bootstrap confidence interval resampling over the 20 maps
  (2000 resamples, Python stdlib `random`/`statistics` only -- no SciPy,
  no ML dependency).
- Outputs: `experiments/results/matches.csv` (one row per match, 360 rows)
  and `experiments/analysis/` (`summary.csv`, `pairwise_comparison.csv`,
  and five plots). The 60 generated map files themselves are scratch
  output (`--maps-dir`, default `/tmp/...`), not committed.

## 5. Results

**360 matches across 60 maps (20 per family), all three strategy pairings,
both sides.** Every match completed in well under a second; the full study
runs in ~3 seconds end to end.

### Win rate by strategy and family (`experiments/analysis/summary.csv`)

| Strategy | Open | Clustered | Chokepoint |
|---|---|---|---|
| fixed | 0.19 | 0.10 | 0.08 |
| strategic | 0.36 | 0.50 | 0.38 |
| hysteresis | 0.45 | 0.50 | 0.50 |

### Pairwise comparison, 95% bootstrap CI over maps (`pairwise_comparison.csv`)

| A vs. B | Family | Mean win-rate diff (A-B) | 95% CI |
|---|---|---|---|
| fixed vs. strategic | open | -0.30 | [-0.53, -0.08] |
| fixed vs. strategic | clustered | -0.60 | [-0.78, -0.38] |
| fixed vs. strategic | chokepoint | -0.48 | [-0.68, -0.30] |
| fixed vs. hysteresis | open | -0.40 | [-0.63, -0.15] |
| fixed vs. hysteresis | clustered | -0.58 | [-0.73, -0.40] |
| fixed vs. hysteresis | chokepoint | -0.60 | [-0.73, -0.48] |
| strategic vs. hysteresis | open | -0.05 | [-0.23, 0.13] |
| strategic vs. hysteresis | clustered | -0.03 | [-0.15, 0.10] |
| strategic vs. hysteresis | chokepoint | -0.23 | [-0.40, -0.05] |

**Interpretation, stated honestly:**

- **`FixedRoleTeamStrategy` loses clearly and consistently** to both
  dynamic strategies on every map family -- every CI above involving
  `fixed` excludes zero. A never-reassigning role allocation is a
  meaningfully worse team strategy in this game, at this map scale.
- **`HysteresisTeamStrategy` vs. `UtilityTeamStrategy` is a mixed,
  map-family-dependent result, not a uniform win**: statistically
  indistinguishable on open and clustered maps (CIs cross zero), but a
  real, significant edge for hysteresis on chokepoint maps specifically
  (CI entirely below zero, i.e. `strategic` loses to `hysteresis` there).
  A plausible mechanism (not independently verified beyond this data):
  chokepoint maps funnel encounters through a narrow passage, where
  briefly losing and re-gaining an Interceptor/Escort commitment every
  time a threat dips near the trigger radius is more costly than it is on
  open ground.
- **Role reassignment frequency** (`role_changes_by_strategy.png`):
  `hysteresis` shows a modest but real reduction versus `strategic`
  (median ~10 vs. ~11-12 role changes per match) -- the commitment
  mechanism is doing something measurable, just not a dramatic one.
- **`fixed` shows markedly more low-progress ticks** (median range
  4400-6000 across families) **than `strategic`/`hysteresis`** (2400-4300)
  -- consistent with a Defender that only ever patrols and an Interceptor
  that only ever chases the nearest enemy having less to actually do, more
  of the time, than a dynamically reassigned team.
- **Map geometry visibly affects outcomes** (`coverage_vs_outcome.png`),
  though this single study does not isolate obstacle coverage as a causal
  factor independent of family identity -- family and coverage are
  correlated by construction (see `docs/PROCEDURAL_GENERATION.md`).

No claim above is extrapolated beyond this data: a different map count,
arena size, or tick budget was not tested and may shift these numbers.

## 6. Ablations

- **Dynamic vs. no reassignment**: `strategic`/`hysteresis` (dynamic) vs.
  `fixed` (never reassigns) -- the headline comparison above.
- **Hysteresis vs. no hysteresis**: `hysteresis` vs. `strategic` -- same
  priority logic, the only difference is role commitment -- the
  family-dependent comparison above.
- **Shared steering**: both new strategies dispatch through the exact same
  `ai::UnitBehaviors` (`moveToward`/`pursueAndAttack`) as `baseline` and
  `strategic` -- no motion-control change was shipped (Section 2), so
  every comparison above isolates strategic-layer differences only, with
  motion control held constant across all four strategies by
  construction.

## 7. Known limitations

- `ai::moveToward`'s overshoot-and-loop behavior near a static target is a
  measured, unresolved limitation (Section 2); an attempted fix did not
  validate and was not shipped.
- This study's map count (20/family) and tick budget (3600) are what this
  milestone's time budget allowed; wider studies (more maps, longer
  matches, varied `SimulationConfig`) could sharpen or overturn the
  chokepoint-specific hysteresis finding.
- Combat (shots fired, eliminations) is rare in most matches studied here;
  this study does not isolate what drives the occasional heavy-combat
  stalemate dynamic observed in one hand-built test scenario.
- `HysteresisTeamStrategy`'s constants (90-tick floor, 1.5x exit
  multiplier) were chosen directly from the diagnosed problem's scale, not
  independently tuned via a parameter sweep.
