# Upstream Audit Reference

This document is the in-repository reference for the architecture audit of
[CAstroParty](https://github.com/kkysen/CAstroParty) (commit `3b3a618`) that
informed AstroParty-AI's design. Source comments throughout this codebase
cite specific sections of this document (e.g. "section 4", "item C-7") to
explain *why* a particular value, mechanic, or architectural boundary was
chosen. It is a condensed version of a longer internal audit conducted
before any AstroParty-AI code was written; it is reproduced here, inside the
public repository, so those citations resolve to a real, readable file
rather than a path on the original author's machine.

No CAstroParty source code or assets are reproduced here or anywhere in this
repository -- see `THIRD_PARTY_NOTICES.md` for the full license and
provenance statement. Everything below is factual description of upstream's
*behavior*, not its source text.

## Section 3 — Player / Ship behavior

Upstream's `Player` struct (`src/player.h`/`.c`) held position, velocity,
acceleration, angle, angular velocity, current button state, and an
embedded `Sprite` (containing an `SDL_Texture*`) in one struct, updated once
per tick in `Player_update()`. Firing was edge-triggered: a bullet spawned
only when the shoot button transitioned from released to pressed
(`button_shoot && !button_shoot_prev`), not merely while held -- a mechanic
AstroParty-AI's `Ship::update()` reproduces directly. The same function
read mouse state for a single local player; there was no notion of multiple
independent local controllers (see Section 8).

## Section 4 — Movement, acceleration, and rotation

The ship accelerated continuously along its current heading every tick --
there was no "stop" or "reverse" control, only steering via a turn button
that always rotated the heading clockwise at a fixed rate. Velocity was
clamped to a maximum magnitude (not per-axis), and position was clamped to
stay within the window. All of this ran on an implicit, uncapped 60 Hz
tick with constants (0.3 units/tick acceleration, 5 degrees/tick turn rate,
4 units/tick max speed) applied directly with no delta-time scaling.
AstroParty-AI's `SimulationConfig` expresses the same constants in
per-second units (multiplying each by the implicit 60 Hz) so that, stepped
at exactly 1/60 s, the numeric effect is identical to upstream while
remaining correct under a different configured tick rate.

Upstream's direction-angle convention placed 0 degrees facing "up" on
screen (negative Y) with angles increasing clockwise, computed as
`cos/sin(degrees - 90)`. `Vector2::fromAngleDegrees()` in this project uses
the identical convention for the same reason: it is a natural "facing up"
default under SDL's top-left-origin, Y-down coordinate system.

## Section 6 — Collision detection

Upstream checked each bullet against each player with a simple
distance-squared-versus-radius-squared test (`Vector_in_radius2`),
excluding a player's own bullets from damaging it. Bullets were treated as
dimensionless points; only players had a collision radius (derived from
their loaded sprite's pixel dimensions). AstroParty-AI's
`physics::isPointWithinRadius()` and `resolveShipBulletCollisions()`
reproduce this exact model, substituting a configured
`shipCollisionRadius` for the sprite-derived value, since this project has
no textures to measure.

## Section 8 — Input handling

Upstream read raw SDL mouse button events for exactly one local player per
process (right-click to turn, left-click to shoot), identified by a
`Client_player_index` set during its networking handshake. There was no
abstraction separating "a decision source" from "the simulation" -- input
state was written directly into global variables the simulation read.
AstroParty-AI's `IShipController` interface exists specifically to let any
number of independent decision sources (human keyboard input, scripted
patterns, or rule-based AI) drive a ship through one shared action type,
with no direct access to mutate simulation state.

## Section 10 — Object creation and destruction, and initial spawn layout

Upstream's `ObjectHandler` held players and bullets in fixed-size static
arrays (capacity 1000 and 6400 respectively) with a swap-remove-with-last
compaction scheme on elimination. Initial spawn placement used a hardcoded
four-corner table for up to four players, falling back to a random
position for any additional players, and every player spawned facing a
uniform default heading (180 degrees) regardless of which corner it
occupied. AstroParty-AI's `World` reproduces the swap-remove concept where
applicable but sizes its ship roster to whatever a loaded `MapDefinition`
provides, and (for its own built-in default two-ship map) deliberately
angles each spawn toward the arena's center rather than copying upstream's
uniform default heading, so a freshly started or restarted match reads
clearly instead of both ships immediately flying toward a screen edge.

## Section 12 — Match state, scoring, and victory conditions

Upstream had no dedicated match-state type. A "last player alive" win
condition was checked inline inside the object-update loop, using a
60-frame `you_win_timer` countdown that, once expired, set a restart flag
and then reset the timer to an arbitrary large negative sentinel value --
a workaround the original author's own code comments acknowledged as
unclean. That mechanism also had no explicit "draw" outcome for the case
where every remaining ship was eliminated in the same frame; it simply
fell through to the same restart path as an ordinary win. AstroParty-AI's
`MatchState` (an explicit `Undecided / ShipWon / Draw` outcome, evaluated
as its own isolated rule rather than inline in the update loop) replaces
this, adding the missing draw case and removing the sentinel-value hack
entirely.

## Issues and Risks — item C-7: simulation/rendering coupling

Upstream's `Player` and `Bullet` structs embedded an `SDL_Texture*`
directly inside the same struct used for physics simulation, and its input
handling wrote straight into the simulation's global state with no
intervening interface. This made the simulation impossible to run, or even
compile independently, without SDL. AstroParty-AI's layering (`astroparty_sim`
has zero SDL dependency; rendering and input are separate, SDL-only
libraries that only ever read or produce data, never embed simulation
entities) exists specifically to make that coupling structurally
impossible to reintroduce -- it is what makes `AstroPartyAI-headless`
possible at all.

## Migration plan and roadmap, summarized

The original audit's migration plan classified every upstream component as
**KEEP**, **ADAPT**, **REWRITE**, or **DROP**. In summary: the core physics
*algorithm* (continuous acceleration, clockwise-only turning, speed
clamping, point-vs-circle collision) was judged sound and was **ADAPTED**
into this project's `Ship`/`Bullet`/`physics` code; upstream's networking
(a non-authoritative TCP relay with no server-side simulation), global
mutable state, and dynamic 1000-capacity object arrays were judged
unsuitable for an AI-experimentation platform and were **DROPPED** or
**REWRITTEN** respectively. The planned milestone sequence that followed
from this audit -- (1) a minimal playable game with simulation/rendering
separation, (2) a shared controller interface, headless execution, and
configuration, (3) playable arenas, team modes, and rule-based AI, (4)
procedural map generation -- is the roadmap this repository has followed
and continues to follow; see `docs/ARCHITECTURE.md`'s roadmap section for
current status.
