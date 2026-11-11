#include "astroparty/ai/UnitBehaviors.hpp"

#include <algorithm>
#include <cmath>
#include <optional>

#include "astroparty/ai/Steering.hpp"
#include "astroparty/physics/Arena.hpp"

namespace astroparty::ai {

namespace {
constexpr float kAlignmentToleranceDegrees = 8.0f;
constexpr float kShootAlignmentToleranceDegrees = 3.0f;
constexpr float kAttackStandoffDistance = 110.0f;
constexpr float kObstacleLookaheadDistance = 150.0f;
constexpr float kObstacleClearance = 10.0f;
constexpr float kArrivalRadius = 4.0f;
constexpr float kVelocityResponseSeconds = 0.4f;
constexpr float kEvadeRadius = 150.0f;
constexpr float kEvadeLookaheadSeconds = 0.45f;
constexpr float kMaxEvasionTurnDegrees = 50.0f;

float dot(core::Vector2 a, core::Vector2 b) noexcept {
    return a.x * b.x + a.y * b.y;
}

void turnToward(core::ShipAction& action, float current, float desired,
                float tolerance = kAlignmentToleranceDegrees) noexcept {
    const float rotation = signedRotationNeeded(current, desired);
    action.turnLeft = rotation < -tolerance;
    action.turnRight = rotation > tolerance;
}

std::optional<core::Vector2> shotIntercept(core::Vector2 from, core::Vector2 target, core::Vector2 velocity,
                                          const core::SimulationConfig& config) noexcept {
    const auto delta = target - from;
    const float speed = config.bulletSpeedUnitsPerSecond;
    const float muzzle = config.bulletSpawnOffset;
    const float gap = delta.lengthSquared() - muzzle * muzzle;
    if (gap <= 0.0f) {
        return std::nullopt;  // The shot would start past the target.
    }
    // Solve |delta + velocity * t| = muzzle + speed * t for the earliest hit.
    const float a = speed * speed - velocity.lengthSquared();
    const float b = muzzle * speed - (delta.x * velocity.x + delta.y * velocity.y);
    const float discriminant = b * b + a * gap;
    if (discriminant < 0.0f) {
        return std::nullopt;
    }
    const float denominator = b + std::sqrt(discriminant);
    if (denominator <= 0.0f) {
        return std::nullopt;
    }
    const float time = gap / denominator;
    return time <= config.bulletLifetimeSeconds ? std::optional{target + velocity * time} : std::nullopt;
}

std::optional<float> travelHeading(const entities::Ship& self, float desired, float distance,
                                   const std::vector<core::ObstacleDefinition>& obstacles,
                                   float shipRadius) noexcept {
    const auto clear = [&](float heading) {
        return !isObstacleAhead(self.position(), heading, shipRadius, obstacles,
                                std::min(distance, kObstacleLookaheadDistance), kObstacleClearance);
    };
    if (clear(desired)) {
        return desired;
    }
    // Try small deviations first, preferring the side we already face on ties.
    const float side = signedRotationNeeded(desired, self.headingDegrees()) < 0.0f ? -1.0f : 1.0f;
    for (float offset = 15.0f; offset <= 180.0f; offset += 15.0f) {
        for (const float heading : {desired + side * offset, desired - side * offset}) {
            if (clear(heading)) {
                return heading;
            }
        }
    }
    return std::nullopt;
}
}  // namespace

core::ShipAction moveToward(const entities::Ship& self, core::Vector2 targetPosition,
                             const std::vector<core::ObstacleDefinition>& obstacles,
                             const core::SimulationConfig& config) noexcept {
    core::ShipAction action;
    const float distance = (targetPosition - self.position()).length();
    const float speed = self.velocity().length();
    const float acceleration = config.shipAccelerationUnitsPerSecondSquared;
    if (distance <= kArrivalRadius) {
        action.throttle = speed > 0.0f ? -1.0f : 0.0f;
        return action;
    }

    const auto heading = travelHeading(self, headingToward(self.position(), targetPosition), distance,
                                       obstacles, config.shipCollisionRadius);
    if (!heading) {
        action.throttle = -1.0f;
        return action;
    }
    // Aim for a speed from which we can stop near the goal, then use the
    // difference from our actual velocity to compensate for sideways drift.
    const float approachSpeed = std::min(config.shipMaxSpeedUnitsPerSecond,
                                        std::sqrt(2.0f * acceleration * (distance - kArrivalRadius)));
    const core::Vector2 desiredVelocity = core::Vector2::fromAngleDegrees(*heading) * approachSpeed;
    const core::Vector2 correction = desiredVelocity - self.velocity();
    const float effort = std::min(1.0f, correction.length() / (acceleration * kVelocityResponseSeconds));
    if (speed > 0.0f) {
        const float velocityHeading = headingToward({}, self.velocity());
        const float stoppingDistance = speed * speed / (2.0f * acceleration);
        if (isObstacleAhead(self.position(), velocityHeading, config.shipCollisionRadius, obstacles,
                            stoppingDistance + speed * kVelocityResponseSeconds, kObstacleClearance)) {
            turnToward(action, self.headingDegrees(), *heading);
            action.throttle = -1.0f;
            return action;
        }
        if (effort > 0.01f &&
            std::abs(signedRotationNeeded(velocityHeading, headingToward({}, correction))) >= 165.0f) {
            turnToward(action, self.headingDegrees(), *heading);
            action.throttle = -effort;
            return action;
        }
    }
    if (effort > 0.01f) {
        turnToward(action, self.headingDegrees(), headingToward({}, correction));
        action.throttle = (action.turnLeft || action.turnRight) ? 0.0f : effort;
    }
    return action;
}

core::ShipAction moveWhileAvoidingEnemy(const entities::Ship& self, core::Vector2 targetPosition,
    const entities::Ship& enemy, const std::vector<core::ObstacleDefinition>& obstacles,
    const core::SimulationConfig& config) noexcept {
    const auto toGoal = targetPosition - self.position();
    const auto toEnemy = enemy.position() - self.position();
    const auto relativeVelocity = enemy.velocity() - self.velocity();
    const float closing = dot(toEnemy, relativeVelocity);
    const float goalDistance = toGoal.length();

    // Finish a nearby pickup or delivery and ignore enemies behind us
    // when the gap is already widening.
    if (goalDistance > config.resourcePickupRadius && (dot(toGoal, toEnemy) >= 0.0f || closing < 0.0f)) {
        const float relativeSpeedSquared = relativeVelocity.lengthSquared();
        const float approachTime = relativeSpeedSquared > 0.0f
            ? std::clamp(-closing / relativeSpeedSquared, 0.0f, kEvadeLookaheadSeconds)
            : 0.0f;
        const auto closestOffset = toEnemy + relativeVelocity * approachTime;
        const float urgency = std::max(0.0f, 1.0f - closestOffset.length() / kEvadeRadius);
        if (urgency > 0.0f) {
            const float goalHeading = headingToward({}, toGoal);
            const float enemySide = signedRotationNeeded(goalHeading, headingToward({}, toEnemy));
            // A head-on encounter chooses right. Otherwise turn away from
            // the enemy's side of the route.
            const float dodge = (enemySide > 0.0f ? -1.0f : 1.0f) * urgency * kMaxEvasionTurnDegrees;
            const auto detour = self.position() + core::Vector2::fromAngleDegrees(goalHeading + dodge) * goalDistance;
            targetPosition = physics::clampToArena(detour, config.arenaSize, config.shipCollisionRadius);
        }
    }
    return moveToward(self, targetPosition, obstacles, config);
}

core::ShipAction pursueAndAttack(const entities::Ship& self, core::Vector2 targetPosition,
                                  const std::vector<core::ObstacleDefinition>& obstacles, const core::SimulationConfig& config,
                                  float engagementRange, core::Vector2 targetVelocity) noexcept {
    core::ShipAction action = moveToward(self, targetPosition, obstacles, config);
    const float distance = (targetPosition - self.position()).length();
    if (distance > engagementRange) {
        return action;
    }
    const auto aimPoint = shotIntercept(self.position(), targetPosition, targetVelocity, config);
    if (!aimPoint) {
        return action;
    }
    const float aimHeading = headingToward(self.position(), *aimPoint);
    if (isObstacleAhead(self.position(), aimHeading, 0.0f, obstacles,
                        (*aimPoint - self.position()).length(), 0.0f)) {
        return action;
    }

    // Face the firing solution once we have a clear shot. Keep closing from
    // far away and brake as we reach firing distance.
    const float tolerance = std::max(kShootAlignmentToleranceDegrees,
                                     0.5f * config.shipTurnRateDegreesPerSecond * config.fixedDeltaSeconds());
    turnToward(action, self.headingDegrees(), aimHeading, tolerance);
    action.shoot = !action.turnLeft && !action.turnRight;
    const float acceleration = config.shipAccelerationUnitsPerSecondSquared;
    const float desiredSpeed = std::sqrt(2.0f * acceleration * std::max(0.0f, distance - kAttackStandoffDistance));
    const float throttle = std::clamp((desiredSpeed - self.velocity().length()) /
                                      (acceleration * kVelocityResponseSeconds), -1.0f, 1.0f);
    if (action.throttle < 0.0f) {
        action.throttle = std::min(action.throttle, throttle);
    } else {
        action.throttle = action.shoot ? throttle : std::min(0.0f, throttle);
    }
    return action;
}

}  // namespace astroparty::ai
