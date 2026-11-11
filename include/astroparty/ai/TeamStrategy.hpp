#pragma once

#include <optional>
#include <vector>

#include "astroparty/core/Vector2.hpp"
#include "astroparty/core/World.hpp"

namespace astroparty::ai {

enum class ShipRole { Collector, Defender, Interceptor, Escort, SelfDefense };

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
    // Optional enemy to pursue for a combat role or avoid for a Collector.
    std::optional<int> targetShipId{};
    // Ally the escort should stay close enough to protect.
    std::optional<int> protectedShipId{};
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
