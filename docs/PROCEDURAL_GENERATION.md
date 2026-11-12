# Procedural Map Generation (Milestone 4)

This document covers the `procgen/` module: a parameterized procedural
generator for Team Resource Battle maps, an automatic validity checker, a
bounded generate/validate/accept/repair/reject pipeline, and an
expressive-range analysis of what it actually produces. It assumes
familiarity with `docs/ARCHITECTURE.md` (particularly the "Game modes" and
"Obstacles" sections) and does not repeat that material.

**Explicitly out of scope for this milestone** (per the Milestone 4
instructions): the rule-based AI (`ai/`) is unchanged -- it is used only as
an integration/playability diagnostic on generated maps, never retuned to
make generated maps "look better." No reinforcement learning is introduced
anywhere in this milestone.

## Three notions of validity

Everything in this module is organized around a distinction this project
never blurs:

1. **Structural validity** -- is the map *shaped* correctly? Unchanged from
   Milestone 3: `core::MapDefinition::validate()` and
   `core::validateTeamResourceBattleStructure()` (the latter extracted from
   `World`'s constructor in this milestone so both `World` and the
   generator's validity checker enforce the identical rule).
2. **Geometric reachability** -- is there *physically room* for a
   ship-sized circle to get from one important location to another?
   Modeled with a coarse occupancy grid (`procgen::OccupancyGrid`) and
   checked by `procgen::checkGeometricReachability()`.
3. **Empirical AI playability** -- does a *real ship* (continuous
   acceleration, clockwise-only turning -- it cannot strafe, reverse, or
   pivot in place) actually get there, driven by the existing, unmodified
   AI? This is a diagnostic run on already-accepted maps, never a gate in
   the generation pipeline. A geometrically connected map is not proof a
   ship can fly the route: a narrow S-bend a grid path crosses trivially
   may be unflyable at this project's turn rate and speed. See "AI
   playability diagnostic" below for what was actually observed.

## The generation algorithm (`MapGenerator.hpp`)

`procgen::generateCandidate(config, rng)` builds one candidate map:

- **Home regions and ship spawns** are placed by a fixed formula (one home
  per team near its edge of the arena, `shipsPerTeam` spawns evenly spaced
  vertically just outside it), mirroring the existing `team_arena.json`
  convention. Nothing about this milestone asks base/spawn placement itself
  to vary -- only obstacles and resources do.
- **Obstacles** are either scattered independently
  (`clusterCount == 0`) or grouped into `clusterCount` clusters, each
  obstacle sampled with a Gaussian offset (`clusterSpreadRadius`) around its
  cluster's center. Restricting where cluster *centers* may land
  (`clusterCenterBandWidthFraction`, a fraction of the contested width
  centered on the arena's vertical midline) is what turns the identical
  mechanism into an "open," "clustered," or "chokepoint" arena -- three
  *parameter values*, not three code paths (see "Map families" below).
- **Resources** are scattered across the contested middle of the arena,
  clear of both home regions' safe zones.
- **Symmetry**: `symmetric = true` (the default) generates only the left
  half directly and mirrors each object across the vertical midline, which
  *guarantees* exact structural symmetry rather than merely similar-looking
  randomness; `symmetric = false` generates both halves independently.
- **Placement**: each object is placed by bounded rejection sampling --
  resample a new candidate position (and radius, for obstacles) up to
  `maxPlacementAttemptsPerObject` times against every already-placed
  obstacle/resource/ship-spawn/home-region, honoring `minObstacleSpacing`
  and `minBaseSeparationFromHome`. If every attempt fails, that one object
  is simply omitted -- `generateCandidate()` never loops unboundedly and
  never throws for a well-formed config, at the cost of sometimes placing
  fewer objects than requested.

### Parameters (`MapGeneratorConfig.hpp`)

| Parameter | Meaning |
|---|---|
| `arenaSize` | Playfield dimensions |
| `shipsPerTeam`, `shipCollisionRadius` | Must match the `SimulationConfig` the map will actually be played with |
| `homeRegionRadius`, `baseMarginFraction` | Home region size and how far in from the arena edge |
| `resourceCount` | Target number of resource spawn points |
| `obstacleCount` | Target number of obstacles |
| `obstacleRadiusMin` / `Max` | Per-obstacle radius range |
| `clusterCount` | 0 = scatter; N >= 1 = group into N Gaussian clusters |
| `clusterSpreadRadius` | Gaussian standard deviation around a cluster center |
| `clusterCenterBandWidthFraction` | Restricts cluster-center placement toward the midline (small = chokepoint-like) |
| `minObstacleSpacing` | Minimum gap between any two obstacles' edges, and between an obstacle and a ship spawn |
| `minBaseSeparationFromHome` | Minimum gap between an obstacle's edge and either home region |
| `symmetric` | Mirror the left half (exact symmetry) vs. generate both sides independently |
| `maxPlacementAttemptsPerObject` | Bounded per-object retry budget |

A known, documented rounding behavior: under `symmetric = true`, an odd
`obstacleCount`/`resourceCount` is effectively rounded **up** to the next
even number (each attempt mirrors into a pair; landing exactly on the
midline has probability zero under continuous sampling).

### Map families

| Family | Key parameter values | What it produces |
|---|---|---|
| Open (`configs/procgen/open.json`) | `clusterCount: 0`, sparse `obstacleCount`, generous `minObstacleSpacing` | Scattered obstacles, generous open space |
| Clustered (`configs/procgen/clustered.json`) | `clusterCount: 4`, `clusterCenterBandWidthFraction: 1.0` | Obstacles grouped into several clusters spread across the whole contested width |
| Chokepoint (`configs/procgen/chokepoint.json`) | `clusterCount: 2`, `clusterCenterBandWidthFraction: 0.2` | Obstacles forced into 1-2 tight groups near the midline, creating narrow passages |

These are verified, not just asserted: see "Expressive-range results" below.

## Geometric validity (`MapValidity.hpp`, `OccupancyGrid.hpp`)

`procgen::checkGeometricReachability(map, shipClearanceRadius, cellSize)`
builds one `OccupancyGrid` (a cell is blocked if a circle of
`shipClearanceRadius` centered there would touch an obstacle or leave the
arena -- the same circular-clearance model obstacle collision already
uses) and checks, via Dijkstra over 8-connected free cells:

- every ship spawn can reach its own team's home region;
- every resource spawn point is reachable from **both** home regions
  (grid connectivity is symmetric, so this also proves a return trip from
  the resource back home is geometrically possible, without a second
  check).

This assumes the map already passed structural validation and does not
re-check that.

## Repair and the generation pipeline

`procgen::repairForReachability()` (`MapRepair.hpp`) is a deliberately
simple heuristic: find whichever connectivity requirement is still
unsatisfied, remove whichever obstacle lies closest to the straight line
between the two points that need connecting, recheck, repeat up to
`maxRemovals` times. It never modifies anything but obstacles, and reports
failure honestly rather than returning a map it could not actually fix.
Structural invalidity is never repaired -- only geometric disconnection is
(see `GenerationPipeline.hpp`'s doc comment for why: `generateCandidate()`'s
fixed home/spawn formula makes a genuine structural failure practically
unreachable, so a structural failure would indicate a config edge case
worth investigating, not something to paper over with a repair heuristic).

`procgen::generateMaps()` (`GenerationPipeline.hpp`) ties it together:

```
generate -> structural validity -> geometric reachability
    -> accept (both passed) / repair (only geometric failed)
       -> accept (repair restored reachability) or reject
```

bounded by `targetAcceptedCount` maps accepted **or** `maxAttempts`
candidates tried, whichever comes first -- this loop never runs
unboundedly. `GenerationStats` records every outcome
(`structurallyInvalid` / `geometricallyInvalid` / `validRaw` / `repaired` /
`rejected` / `accepted`), with the invariants
`structurallyInvalid + geometricallyInvalid + validRaw == totalAttempts`
and `accepted + rejected == totalAttempts` always holding -- nothing is
hidden from the statistics. `rawValidityRate()` (valid with no repair) and
`postRepairAcceptanceRate()` (valid-or-repaired) are reported separately,
since conflating them would hide how much repair is actually doing.

Deliberately exercising this against an adversarial config
(`tests/test_generation_pipeline.cpp`'s `denseRepairProneConfig`: a
near-solid, minimally-spaced obstacle scatter) confirms repair and
rejection both actually happen; the three shipped presets and the
expressive-range study below are all comfortably non-adversarial and
achieved a 100% raw validity rate in practice, so repair is a safety net
exercised rarely by sensible configs, not a crutch the presets depend on.

## Expressive-range metrics (`MapMetrics.hpp`)

Computed from a map's **actual final geometry**, never from the config
that targeted it (placement can fail and be skipped; repair can remove
obstacles):

- `obstacleCoverageRatio` -- exact analytic sum-of-circle-areas / arena
  area.
- `team{0,1}NavigationPathLength` -- average grid-shortest-path length
  (`OccupancyGrid::shortestPathLength`) from each team's home to every
  resource.
- `teamAccessibilityImbalance` -- `|team0 - team1|` path length.
- `obstacleClusterCount` -- a union-find over actual obstacle adjacency
  (gap between edges below `clusterGapThreshold`), independent of and a
  cross-check against the generator's own `clusterCount` knob.
- `reachableFreeSpaceFraction` -- fraction of all free grid cells reachable
  from team 0's home; lower than 1.0 means obstacles walled off an open
  pocket the required-path check doesn't itself notice.

## Expressive-range results

`scripts/analyze_generated_maps.py` orchestrates `AstroPartyAI-mapgen`
(subprocess calls only; no generation/validity logic is reimplemented in
Python) and plots the result with matplotlib. Outputs committed under
`docs/procgen_analysis/`:

- `all_maps_metrics.csv` -- every accepted map's metrics, 205 rows total.
- `family_summary.csv` -- per-family mean/stdev of every metric.
- `generation_stats.json` -- `GenerationStats` for every run.
- `family_obstacleCoverageRatio_histogram.png`,
  `family_obstacleClusterCount_histogram.png`,
  `family_teamAccessibilityImbalance_boxplot.png` -- family comparisons.
- `density_sensitivity_coverage.png` -- `obstacleCount` at three levels
  (4/14/26, otherwise-open config) on `obstacleCoverageRatio`.
- `symmetry_sensitivity_imbalance.png` -- `symmetric` true/false on
  `teamAccessibilityImbalance`.
- `raw_validity_rates.png` -- raw validity / post-repair acceptance per run.

**Reproduction** (from the repository root, after building):

```sh
cmake --build build -j
python3 scripts/analyze_generated_maps.py \
  --executable build/AstroPartyAI-mapgen \
  --configs-dir configs/procgen \
  --maps-output-dir /tmp/astroparty_procgen_maps \
  --analysis-dir docs/procgen_analysis \
  --count-per-family 40
```

**Findings** (120 maps across the three families, 45 for the density
sweep, 40 for the symmetry sweep -- 205 accepted maps total, all raw-valid,
no repair needed):

- **The three families are measurably different**, from identical
  generator code: mean `obstacleCoverageRatio` 0.019 (open) / 0.054
  (clustered) / 0.046 (chokepoint); mean `obstacleClusterCount` 5.3 / 6.9 /
  1.4 -- chokepoint reliably collapses into 1-2 tight obstacle groups,
  exactly the effect `clusterCenterBandWidthFraction` is meant to produce.
- **`obstacleCount` has a strong, clean, monotonic effect** on
  `obstacleCoverageRatio`: the three density levels' distributions do not
  overlap at all (see `density_sensitivity_coverage.png`).
  **`clusterCenterBandWidthFraction` has a strong effect on
  `obstacleClusterCount`** (see the family comparison above) -- both are
  the "meaningful parameter changes, not hardcoded templates" the spec
  asks the three families to come from.
- **`symmetric` has a dramatic effect** on `teamAccessibilityImbalance`:
  median ~0 (max ~10, grid-discretization noise) when `symmetric = true`,
  vs. median ~160 (up to ~435) when `false` -- an unambiguous, large effect
  from a single boolean.
- **What the generator cannot (currently) produce**: arena shapes other
  than a rectangle, non-circular obstacles, obstacle placements correlated
  with ship spawn positions beyond simple clearance, or any variation in
  the home-region/spawn-point layout itself (all formulaic, by design --
  see "The generation algorithm" above).
- **Common generation failures**: the only failure mode observed in
  practice is a single obstacle/resource placement attempt exhausting its
  retry budget in a tightly-packed region (silently omitted, reducing that
  one candidate's actual object count below the target); this grows more
  likely as `obstacleCount` rises relative to the arena's area and as
  `minObstacleSpacing`/`minBaseSeparationFromHome` grow. No candidate in
  this study was ever rejected outright.
- **How validity constraints affect diversity**: `minBaseSeparationFromHome`
  and the ship-spawn clearance requirement both carve out a fixed "safe
  zone" near each home that no obstacle configuration can encroach on,
  regardless of family -- this is a deliberate, constant constraint on
  diversity (a home region is never besieged), not a side effect of any
  one family's parameters.

No claim above is extrapolated beyond this data; a different arena size,
ship radius, or parameter range was not tested and may behave differently.

## AI playability diagnostic

Per the Milestone 4 instructions, the existing rule-based AI
(`BaselineTeamStrategy`/`UtilityTeamStrategy`/`TeamShipController`) was run,
**unmodified**, against one representative map from each family
(`configs/procgen/examples/`) as an integration and playability check, not
a validity gate:

- All three maps load correctly in both the headless runner
  (`AstroPartyAI-headless --map configs/procgen/examples/<family>_example.json`)
  and the graphical client (`AstroPartyAI --mode team --map ...`), with
  ships and resources spawning correctly and no crash.
- All three matches ran to the tick limit (`Timeout`) in a short
  (600-1800 tick) smoke test with both teams collecting and delivering at
  least one resource each -- the AI is not stuck or non-functional on any
  of the three families.
- No ship was eliminated in any of these particular short runs (all three
  `shipsDestroyed` arrays stayed `[0, 0]`) -- consistent with (not
  necessarily caused by) this project's rule-based AI being reactive
  rather than aggressively seeking out engagements; this is reported as an
  observation for Milestone 5, not diagnosed further here, per this
  milestone's explicit "do not redesign the AI agents" scope.
- This is a small, illustrative smoke test (three maps, short runs), not a
  systematic study of AI behavior across the full expressive range; a
  larger-scale version of this check (many maps per family, longer runs,
  explicitly looking for stuck-ship or zero-progress cases) is recommended
  for Milestone 5, where AI algorithms themselves are in scope to study and
  compare.

## Testing

`tests/test_rng.cpp`, `test_map_generator_config.cpp`, `test_occupancy_grid.cpp`,
`test_map_generator.cpp`, `test_map_validity.cpp`, `test_map_repair.cpp`,
`test_generation_pipeline.cpp`, and `test_map_metrics.cpp` cover: RNG
determinism, config validation, grid connectivity/path-length correctness
against hand-built blocked/open maps, generator determinism and seed
variation, structural/placement correctness (no overlaps, arena bounds,
base separation) including deliberately adversarial edge cases (near-solid
obstacle density, a minimal arena, a narrow chokepoint band), geometric
validity against hand-crafted connected/disconnected maps, repair
restoring reachability (and honestly failing when it cannot), the full
pipeline's accounting invariants and bounded termination, and metrics
against geometries with known, hand-computed expected values. All tests
are fully seeded/deterministic -- no unseeded randomized test exists
anywhere in this suite.

## Known limitations

- Arena shape is always an axis-aligned rectangle; obstacles are always
  circles. Neither is parameterized beyond that.
- Base/spawn layout is a fixed formula, not itself procedurally varied.
- The occupancy grid is a discretization (`cellSize`), so reported path
  lengths are grid-resolution approximations, not exact Euclidean
  geodesics.
- Geometric reachability is not proof of AI (or human) playability -- see
  "Three notions of validity" above and the diagnostic results, which are
  illustrative, not an exhaustive study.
- The repair heuristic only ever removes obstacles; it cannot "repair" a
  map that is invalid for a reason other than obstacle-caused
  disconnection (e.g. too few resource spawn points were successfully
  placed).
