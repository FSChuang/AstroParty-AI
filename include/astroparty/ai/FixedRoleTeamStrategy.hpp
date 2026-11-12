#pragma once

#include "astroparty/ai/TeamStrategy.hpp"

namespace astroparty::ai {

// Milestone 5's "Variant A": a literally fixed per-ship role assignment,
// decided once by each ship's position in teamShipIds and never reassigned
// for any strategic reason. The only exceptions are the same ones every
// other strategy in this project already makes for events the game rules
// themselves force a response to, not a strategic choice:
//
// - An eliminated ship gets the same harmless Defender/home placeholder
//   goal every strategy gives a dead ship (its action is a no-op anyway;
//   see ai::TeamShipController).
// - A ship currently carrying a resource always heads home to deliver --
//   refusing to would be nonsensical (a "Collector" that will not ever
//   deliver is not a Collector), and every existing strategy in this
//   project treats this the same way.
//
// Role by position (cycling if teamShipIds.size() is not a multiple of 3):
// position 0 = Collector, position 1 = Defender, position 2 = Interceptor.
// Unlike BaselineTeamStrategy (which already exists and is deliberately
// simpler than UtilityTeamStrategy, but still has the Defender reactively
// engage a nearby enemy), this strategy's Defender only ever patrols, and
// its Interceptor always pursues whichever enemy ship is nearest -- there
// is no "is this worth reacting to" judgment anywhere in this class, by
// design.
class FixedRoleTeamStrategy final : public ITeamStrategy {
public:
    [[nodiscard]] std::vector<ShipGoal> assignGoals(const core::World& world, int teamId,
                                                      const std::vector<int>& teamShipIds) override;
};

}  // namespace astroparty::ai
