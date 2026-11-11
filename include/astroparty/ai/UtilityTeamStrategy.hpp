#pragma once

#include "astroparty/ai/TeamStrategy.hpp"

namespace astroparty::ai {

// This project's strategic AI: a simple, explainable, utility/priority-based
// role assignment, recomputed from scratch every tick (no persisted "sticky"
// assignment state) directly from World's current, publicly observable
// state. Recomputing fresh each tick is what makes dynamic reassignment
// (deliverable requirement) fall out naturally rather than needing separate
// "did something change" event-detection logic: a dead ship, a newly
// available resource, or an approaching enemy carrier simply produce a
// different assignment the next time assignGoals() is called.
//
// Priority order (highest first), each consuming ships from a shared pool
// of this team's not-yet-assigned, alive ships:
//
//   1. Collector (deliver): any ship already carrying a resource always
//      heads home, regardless of anything else happening in the match.
//   2. Reserve a support ship when two ships can collect or deliver. It
//      escorts threatened allies first, then uses the team's support policy.
//      Collectors split available cores when possible.
//   3. If the formation is short a ship, escort a threatened carrier when
//      possible.
//   4. Interceptor: if an enemy ship carrying a resource is within
//      threatening range of this team's home region, the nearest
//      available ship is sent to stop it.
//   5. Collector or Defender: remaining ships gather or patrol home. An
//      unladen ship in either role can switch to close-range self-defense.
//
// See docs/ARCHITECTURE.md's "Team-level strategic AI" section for the
// full rationale, including why a stateless, recomputed-every-tick design
// was chosen over an explicit event/state-machine model.
class UtilityTeamStrategy : public ITeamStrategy {
public:
    UtilityTeamStrategy() = default;
    [[nodiscard]] std::vector<ShipGoal> assignGoals(const core::World& world, int teamId,
                                                      const std::vector<int>& teamShipIds) override;

protected:
    explicit UtilityTeamStrategy(float escortThreatRadius) : escortThreatRadius_(escortThreatRadius) {}
    [[nodiscard]] virtual ShipGoal supportGoal(const core::World& world, int teamId, const entities::Ship& ship,
                                               core::Vector2 homePosition, std::optional<int> enemyCarrierId) const;

private:
    float escortThreatRadius_ = 250.0f;
};

}  // namespace astroparty::ai
