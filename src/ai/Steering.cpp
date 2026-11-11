#include "astroparty/ai/Steering.hpp"

#include <algorithm>
#include <cmath>

namespace astroparty::ai {

using core::Vector2;

namespace {
constexpr float kPi = 3.14159265358979323846f;

float normalizeDegrees(float degrees) noexcept {
    float normalized = std::fmod(degrees, 360.0f);
    if (normalized < 0.0f) {
        normalized += 360.0f;
    }
    return normalized;
}
}  // namespace

float headingToward(Vector2 from, Vector2 to) noexcept {
    const Vector2 delta = to - from;
    if (delta.lengthSquared() < 1e-9f) {
        return 0.0f;
    }
    // Inverse of Vector2::fromAngleDegrees's convention (degrees - 90 before
    // cos/sin): solve for degrees given a direction vector.
    const float radians = std::atan2(delta.y, delta.x);
    return normalizeDegrees(radians * (180.0f / kPi) + 90.0f);
}

float clockwiseRotationNeeded(float currentHeadingDegrees, float targetHeadingDegrees) noexcept {
    return normalizeDegrees(targetHeadingDegrees - currentHeadingDegrees);
}

float signedRotationNeeded(float currentHeadingDegrees, float targetHeadingDegrees) noexcept {
    const float clockwise = clockwiseRotationNeeded(currentHeadingDegrees, targetHeadingDegrees);
    return clockwise > 180.0f ? clockwise - 360.0f : clockwise;
}

bool shouldTurnToFace(float currentHeadingDegrees, float targetHeadingDegrees, float toleranceDegrees) noexcept {
    return std::abs(signedRotationNeeded(currentHeadingDegrees, targetHeadingDegrees)) > toleranceDegrees;
}

bool isObstacleAhead(Vector2 position, float headingDegrees, float travelerRadius,
                      const std::vector<core::ObstacleDefinition>& obstacles, float lookaheadDistance,
                      float clearance) noexcept {
    const Vector2 forward = Vector2::fromAngleDegrees(headingDegrees);

    for (const core::ObstacleDefinition& obstacle : obstacles) {
        const Vector2 toObstacle = obstacle.position - position;
        const float forwardDistance = toObstacle.x * forward.x + toObstacle.y * forward.y;
        if (forwardDistance <= 0.0f && toObstacle.lengthSquared() > 0.0f) {
            continue;  // Moving away, including when starting in the clearance margin.
        }

        const Vector2 closestPointOnPath = position + forward * std::clamp(forwardDistance, 0.0f, lookaheadDistance);
        const float radius = obstacle.radius + travelerRadius + clearance;
        if (core::distanceSquared(obstacle.position, closestPointOnPath) < radius * radius) {
            return true;
        }
    }
    return false;
}

}  // namespace astroparty::ai
