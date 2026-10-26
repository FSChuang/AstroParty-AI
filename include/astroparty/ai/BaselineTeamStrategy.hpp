#pragma once

#include "astroparty/ai/TeamStrategy.hpp"

namespace astroparty::ai {

// The simpler opponent Deliverable 1 requires for comparison against the
// strategic AI (see UtilityTeamStrategy). Deliberately simpler by design:
//
// - Fixed roles: every ship except the last on the team is always a
//   Collector; the last is always a Defender. Unlike UtilityTeamStrategy,
//   roles are never reassigned based on match events (no Interceptor or
//   Escort role exists here at all).
// - Collectors always greedily target the nearest available resource.
// - The Defender patrols its own home region, engaging the nearest enemy
//   only if one strays within a fixed defense radius of it -- basic
//   reactive defense, not coordinated team reasoning.
// - A ship already carrying a resource always heads straight home,
//   regardless of its normal fixed role.
//
// It observes the exact same World through the exact same
// TeamObservations queries as UtilityTeamStrategy -- see
// docs/ARCHITECTURE.md's "Baseline opponent" section for why that parity
// matters for a fair comparison.
class BaselineTeamStrategy final : public ITeamStrategy {
public:
    [[nodiscard]] std::vector<ShipGoal> assignGoals(const core::World& world, int teamId,
                                                      const std::vector<int>& teamShipIds) override;
};

}  // namespace astroparty::ai
