#pragma once

#include <vector>

#include "astroparty/core/Action.hpp"
#include "astroparty/core/MapDefinition.hpp"
#include "astroparty/core/SimulationConfig.hpp"
#include "astroparty/core/Vector2.hpp"
#include "astroparty/entities/Ship.hpp"

namespace astroparty::ai {

// Decides a ShipAction that steers `self` toward `targetPosition`, routing
// around any obstacle directly ahead, and never shoots. Used for
// Collector/Defender-style movement goals, where there is nothing to shoot
// at yet.
[[nodiscard]] core::ShipAction moveToward(const entities::Ship& self, core::Vector2 targetPosition,
                                           const std::vector<core::ObstacleDefinition>& obstacles,
                                           const core::SimulationConfig& config) noexcept;

// Bends a collection or delivery route away from a nearby enemy while
// preserving the destination and pickup behavior.
[[nodiscard]] core::ShipAction moveWhileAvoidingEnemy(const entities::Ship& self, core::Vector2 targetPosition,
    const entities::Ship& enemy, const std::vector<core::ObstacleDefinition>& obstacles,
    const core::SimulationConfig& config) noexcept;

// Closes on the target, then leads its motion and requests a shot when
// aimed and in range with a clear firing lane.
[[nodiscard]] core::ShipAction pursueAndAttack(const entities::Ship& self, core::Vector2 targetPosition,
                                                const std::vector<core::ObstacleDefinition>& obstacles,
                                                const core::SimulationConfig& config, float engagementRange,
                                                core::Vector2 targetVelocity = {}) noexcept;

}  // namespace astroparty::ai
