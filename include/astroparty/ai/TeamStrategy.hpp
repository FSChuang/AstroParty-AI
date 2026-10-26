#pragma once

#include <optional>
#include <vector>

#include "astroparty/core/Vector2.hpp"
#include "astroparty/core/World.hpp"

namespace astroparty::ai {

enum class ShipRole { Collector, Defender, Interceptor, Escort };

// What one ship should be trying to do this tick, as decided by a team
// strategy and handed to TeamShipController, which turns it into an actual
// ShipAction via the unit behaviors in UnitBehaviors.hpp.
//
// This is the entire information flow between the two layers: a strategy
// never produces a ShipAction itself, and a controller never decides roles
// itself -- see docs/ARCHITECTURE.md's "Team-level strategic AI" section.
struct ShipGoal {
    ShipRole role = ShipRole::Defender;
    core::Vector2 targetPosition{};
    // Set for Interceptor (the enemy carrier to stop) and Escort (the
    // friendly carrier to protect); unset for Collector and Defender, whose
    // target is a place, not a ship.
    std::optional<int> targetShipId;
};

// Decides, each tick, what every ship on one team should be trying to do.
//
// Implementations observe `world` through its public const accessors --
// nothing here is privileged or hidden from a baseline implementation, so a
// strategic AI and a baseline AI genuinely compete on equal information
// (see docs/ARCHITECTURE.md's "Baseline opponent" section). Implementations
// return goals only; they never mutate `world` or any ship directly.
class ITeamStrategy {
public:
    virtual ~ITeamStrategy() = default;

    // Returns one goal per ship ID in `teamShipIds`, in the same order.
    [[nodiscard]] virtual std::vector<ShipGoal> assignGoals(const core::World& world, int teamId,
                                                              const std::vector<int>& teamShipIds) = 0;
};

}  // namespace astroparty::ai
