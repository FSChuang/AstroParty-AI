#include "astroparty/ai/Steering.hpp"

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

bool shouldTurnToFace(float currentHeadingDegrees, float targetHeadingDegrees, float toleranceDegrees) noexcept {
    const float needed = clockwiseRotationNeeded(currentHeadingDegrees, targetHeadingDegrees);
    // `needed` near 0 means the target is just ahead (about to be reached by
    // continuing to turn); `needed` near 360 means the target was just
    // passed (turning further would only move away from it before wrapping
    // all the way back around). Both count as "aligned enough."
    const bool alreadyAligned = (needed < toleranceDegrees) || (needed > 360.0f - toleranceDegrees);
    return !alreadyAligned;
}

bool isObstacleAhead(Vector2 position, float headingDegrees, float travelerRadius,
                      const std::vector<core::ObstacleDefinition>& obstacles, float lookaheadDistance,
                      float clearance) noexcept {
    const Vector2 forward = Vector2::fromAngleDegrees(headingDegrees);

    for (const core::ObstacleDefinition& obstacle : obstacles) {
        const Vector2 toObstacle = obstacle.position - position;
        const float forwardDistance = toObstacle.x * forward.x + toObstacle.y * forward.y;
        if (forwardDistance <= 0.0f || forwardDistance > lookaheadDistance) {
            continue;  // Behind us, or farther ahead than we're looking.
        }

        const Vector2 closestPointOnPath = position + forward * forwardDistance;
        const float perpendicularDistance = (obstacle.position - closestPointOnPath).length();
        if (perpendicularDistance < obstacle.radius + travelerRadius + clearance) {
            return true;
        }
    }
    return false;
}

}  // namespace astroparty::ai
