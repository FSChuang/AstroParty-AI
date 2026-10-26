#include "astroparty/ai/UnitBehaviors.hpp"

#include "astroparty/ai/Steering.hpp"

namespace astroparty::ai {

namespace {
// Chosen by eye for ships at this project's arena scale; they carry no
// deeper meaning than "turns off before overshooting" and "only shoots
// when actually lined up."
constexpr float kAlignmentToleranceDegrees = 8.0f;
constexpr float kShootAlignmentToleranceDegrees = 6.0f;
constexpr float kObstacleLookaheadDistance = 150.0f;
constexpr float kObstacleClearance = 10.0f;
}  // namespace

core::ShipAction moveToward(const entities::Ship& self, core::Vector2 targetPosition,
                             const std::vector<core::ObstacleDefinition>& obstacles,
                             float shipCollisionRadius) noexcept {
    core::ShipAction action;

    if (isObstacleAhead(self.position(), self.headingDegrees(), shipCollisionRadius, obstacles,
                         kObstacleLookaheadDistance, kObstacleClearance)) {
        // Simple reactive avoidance: keep turning regardless of the intended
        // target until the straight-ahead path is clear again. This is not
        // path planning -- see Steering.hpp's isObstacleAhead for why that
        // is an accepted simplification at this scale (a handful of
        // obstacles per map, not a dense field requiring real pathfinding).
        action.turnRight = true;
        return action;
    }

    const float desiredHeading = headingToward(self.position(), targetPosition);
    action.turnRight = shouldTurnToFace(self.headingDegrees(), desiredHeading, kAlignmentToleranceDegrees);
    return action;
}

core::ShipAction pursueAndAttack(const entities::Ship& self, core::Vector2 targetPosition,
                                  const std::vector<core::ObstacleDefinition>& obstacles, float shipCollisionRadius,
                                  float engagementRange) noexcept {
    core::ShipAction action = moveToward(self, targetPosition, obstacles, shipCollisionRadius);

    const float desiredHeading = headingToward(self.position(), targetPosition);
    const bool aimed = !shouldTurnToFace(self.headingDegrees(), desiredHeading, kShootAlignmentToleranceDegrees);
    const bool inRange = core::distanceSquared(self.position(), targetPosition) <= engagementRange * engagementRange;
    action.shoot = aimed && inRange;
    return action;
}

}  // namespace astroparty::ai
