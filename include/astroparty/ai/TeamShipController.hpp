#pragma once

#include <memory>
#include <vector>

#include "astroparty/ai/TeamStrategy.hpp"
#include "astroparty/input/ShipController.hpp"

namespace astroparty::ai {

// Adapts a team strategy to the IShipController interface. Each instance
// controls exactly one ship: on decide(), it asks the shared strategy what
// every ship on the team should be doing this tick, extracts its own
// ship's goal, and converts that goal into a ShipAction via the unit
// behaviors in UnitBehaviors.hpp (Interceptor/Escort attack their target;
// Collector/Defender move toward theirs).
//
// Multiple TeamShipControllers on the same team share one ITeamStrategy
// instance (by shared_ptr) and each independently calls assignGoals() with
// identical inputs each tick; since assignGoals() is a pure function of
// World's current state, this redundant recomputation (3 calls instead of
// 1 per tick, for a 3-ship team) is simple, correct, and a complete
// non-issue at this project's scale.
class TeamShipController final : public input::IShipController {
public:
    TeamShipController(std::shared_ptr<ITeamStrategy> strategy, int teamId, std::vector<int> teamShipIds) noexcept;

    [[nodiscard]] core::ShipAction decide(const core::World& world, int controlledShipId) override;

private:
    std::shared_ptr<ITeamStrategy> strategy_;
    int teamId_;
    std::vector<int> teamShipIds_;
};

}  // namespace astroparty::ai
