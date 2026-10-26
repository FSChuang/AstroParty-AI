#pragma once

#include <vector>

#include "astroparty/core/Action.hpp"
#include "astroparty/core/MapDefinition.hpp"
#include "astroparty/core/Vector2.hpp"
#include "astroparty/entities/Ship.hpp"

namespace astroparty::ai {

// Decides a ShipAction that steers `self` toward `targetPosition`, routing
// around any obstacle directly ahead, and never shoots. Used for
// Collector/Defender-style movement goals, where there is nothing to shoot
// at yet.
[[nodiscard]] core::ShipAction moveToward(const entities::Ship& self, core::Vector2 targetPosition,
                                           const std::vector<core::ObstacleDefinition>& obstacles,
                                           float shipCollisionRadius) noexcept;

// Steers `self` toward `targetPosition` (typically an enemy ship's current
// position) the same way moveToward does, and additionally fires whenever
// `self` is both closely aimed at the target and within `engagementRange`
// of it. Used for Interceptor/Escort and any other "attack this" goal.
[[nodiscard]] core::ShipAction pursueAndAttack(const entities::Ship& self, core::Vector2 targetPosition,
                                                const std::vector<core::ObstacleDefinition>& obstacles,
                                                float shipCollisionRadius, float engagementRange) noexcept;

}  // namespace astroparty::ai
