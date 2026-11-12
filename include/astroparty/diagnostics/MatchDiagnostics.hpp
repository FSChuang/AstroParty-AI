#pragma once

#include <array>
#include <memory>
#include <string>
#include <vector>

#include "astroparty/ai/TeamStrategy.hpp"
#include "astroparty/headless/TeamMatchResult.hpp"
#include "astroparty/input/ShipController.hpp"

namespace astroparty::diagnostics {

// Parameters for the "low progress" (informally, "stuck") detector.
//
// A ship in this game accelerates continuously and can never stop, so a
// low INSTANTANEOUS velocity never happens except right after a respawn --
// velocity alone cannot distinguish "purposefully circling near a target"
// from "genuinely failing to make progress." Instead, this detector tracks
// each ship's NET straight-line displacement over a trailing window of
// `windowTicks` ticks: if that displacement stays below
// `minNetDisplacement` for an entire window, the ship is flagged as making
// low progress for that tick, regardless of how fast or how erratically it
// is moving within the window.
struct StuckDetectionConfig {
    int windowTicks = 120;             // 2 seconds at the default 60 Hz tick rate.
    float minNetDisplacement = 50.0f;  // World units the window must cover to NOT count as low progress.
};

// Per-ship counters accumulated over one match. All are precise, cumulative
// event counts for the ship's entire ShipGoal-level operation:
//
// - shotsFired: edge-triggered (ShipAction::shoot transitioning false ->
//   true), i.e. actual bullets fired, not ticks spent holding the button.
//   In this project's one-hit-kill combat model, "successful hits" is
//   already exactly core::World::shipsDestroyed() (see
//   headless::TeamMatchResult) -- there is no separate "hit but survived"
//   state to track, so this file does not duplicate that counter.
// - obstacleCollisionEvents: edge-triggered (the ship transitions from not
//   touching any obstacle to touching one). This project's physics has no
//   ship-vs-ship collision at all (see core::Collisions.hpp) -- the only
//   collision a ship can have with solid geometry is against an obstacle,
//   so that is what this counts.
// - lowProgressTicks: ticks flagged by the StuckDetectionConfig detector
//   above.
// - roleChanges: ticks where this ship's ai::ShipRole (from its team
//   strategy's assignGoals()) differs from the immediately preceding
//   tick's role. The very first tick is never counted as a change.
struct ShipDiagnostics {
    int shotsFired = 0;
    int obstacleCollisionEvents = 0;
    int lowProgressTicks = 0;
    int roleChanges = 0;
};

struct TeamDiagnostics {
    // Indexed by position within the team (0, 1, 2 for a 3-ship team),
    // matching the order of the teamShipIds passed to runInstrumentedTeamMatch.
    std::vector<ShipDiagnostics> ships;

    // One sample per resource this team successfully delivered (not
    // dropped): ticks elapsed between that resource being picked up and
    // delivered. Empty if the team delivered nothing.
    std::vector<int> deliveryLatenciesTicks;
};

struct MatchDiagnostics {
    std::array<TeamDiagnostics, 2> teams;

    [[nodiscard]] std::string toJsonString() const;
};

// Runs a complete Team Resource Battle match exactly like
// headless::runTeamMatch, additionally instrumenting it tick-by-tick to
// produce a MatchDiagnostics. `team0Strategy`/`team1Strategy` are consulted
// once per tick, read-only, purely to observe each ship's assigned
// ai::ShipRole for roleChanges -- pass nullptr for a team not driven by a
// shared ai::ITeamStrategy (that team's roleChanges stays 0 for every ship).
// All other parameters and preconditions match runTeamMatch.
[[nodiscard]] std::pair<headless::TeamMatchResult, MatchDiagnostics> runInstrumentedTeamMatch(
    core::World& world, std::vector<std::unique_ptr<input::IShipController>>& controllers, int maxTicks,
    std::shared_ptr<ai::ITeamStrategy> team0Strategy, std::shared_ptr<ai::ITeamStrategy> team1Strategy,
    StuckDetectionConfig stuckConfig = {});

}  // namespace astroparty::diagnostics
