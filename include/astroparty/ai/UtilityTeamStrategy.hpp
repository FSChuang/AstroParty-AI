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
//   2. Interceptor: if an enemy ship carrying a resource is within
//      threatening range of this team's home region, the nearest
//      available ship is sent to stop it.
//   3. Escort: if this team has its own carrier and an enemy is closing in
//      on it, the nearest remaining available ship is sent to engage that
//      threat.
//   4. Collector (gather): each remaining available ship is sent after the
//      nearest currently-available resource.
//   5. Defender: any ship left unassigned (e.g. no resources are currently
//      available) patrols this team's home region.
//
// See docs/ARCHITECTURE.md's "Team-level strategic AI" section for the
// full rationale, including why a stateless, recomputed-every-tick design
// was chosen over an explicit event/state-machine model.
class UtilityTeamStrategy final : public ITeamStrategy {
public:
    [[nodiscard]] std::vector<ShipGoal> assignGoals(const core::World& world, int teamId,
                                                      const std::vector<int>& teamShipIds) override;
};

}  // namespace astroparty::ai
