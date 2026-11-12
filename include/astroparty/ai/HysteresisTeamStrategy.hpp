#pragma once

#include <unordered_map>

#include "astroparty/ai/TeamStrategy.hpp"

namespace astroparty::ai {

// Milestone 5's "Variant C": UtilityTeamStrategy's same priority logic
// (carrying -> deliver; intercept a threatening enemy carrier; escort a
// threatened friendly carrier; gather the nearest resource; else patrol),
// with one addition -- ROLE COMMITMENT with event-triggered reassignment --
// motivated directly by this milestone's diagnostics
// (docs/AI_EVALUATION.md): UtilityTeamStrategy recomputes its hard-
// threshold Interceptor/Escort triggers from scratch every tick with no
// memory, so a ship hovering near a trigger radius (kInterceptRadius,
// kEscortThreatRadius) can be assigned and unassigned the same role on
// consecutive ticks -- classic threshold oscillation in a stateless
// utility system.
//
// Design:
//   Inputs: the same World observations every strategy in this project
//     uses (ai::TeamObservations) -- no privileged information.
//   Scoring/assignment objective: identical priority order to
//     UtilityTeamStrategy (see its own doc comment) for any ship with no
//     active commitment.
//   Role selection: once a ship is assigned Interceptor or Escort, it
//     COMMITS to that specific role and target for at least
//     kMinCommitmentTicks ticks, continuing to update only the target's
//     current position (not re-running the priority evaluation).
//   Reassignment conditions: after the minimum commitment duration has
//     elapsed, the commitment is dropped (and the ship re-evaluated from
//     the top of the priority order) only when its target is no longer a
//     valid threat -- dead, no longer carrying, or beyond a WIDER exit
//     radius (kInterceptExitMultiplier / kEscortExitMultiplier times the
//     entry radius) than the one that triggered it. This is the
//     hysteresis band: entering and exiting a role use different
//     thresholds, so a ship sitting near the entry radius cannot flicker
//     back and forth every tick. A ship that starts carrying a resource
//     always drops its commitment immediately and heads home regardless
//     of the commitment floor -- delivering is never something to
//     "commit away from."
//   Computational cost: O(teamShipIds.size()) per call, identical order to
//     UtilityTeamStrategy, plus O(1) hash-map bookkeeping per ship for its
//     commitment state.
//
// STATEFUL, unlike every other strategy in this project (UtilityTeamStrategy
// and BaselineTeamStrategy are pure functions of World's current state).
// This matters because ai::TeamShipController calls assignGoals() once per
// ship sharing one ITeamStrategy instance per team -- i.e. up to 3 times
// per actual simulation tick with identical inputs. assignGoals() detects
// and collapses these redundant same-tick calls (keyed on
// world.elapsedTicks()) so commitment timers only ever advance once per
// real tick, regardless of how many ships on the team call it.
class HysteresisTeamStrategy final : public ITeamStrategy {
public:
    [[nodiscard]] std::vector<ShipGoal> assignGoals(const core::World& world, int teamId,
                                                      const std::vector<int>& teamShipIds) override;

private:
    struct Commitment {
        ShipRole role = ShipRole::Interceptor;
        int targetShipId = -1;
        int minTicksRemaining = 0;
    };

    struct CachedTeamResult {
        int teamId = -1;
        int elapsedTicks = -1;
        std::vector<ShipGoal> goals;
    };

    std::unordered_map<int, Commitment> commitmentsByShipId_;
    CachedTeamResult cache_[2];

    [[nodiscard]] std::vector<ShipGoal> computeGoals(const core::World& world, int teamId,
                                                       const std::vector<int>& teamShipIds);
};

}  // namespace astroparty::ai
