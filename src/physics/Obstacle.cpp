#include "astroparty/physics/Obstacle.hpp"

namespace astroparty::physics {

using core::Vector2;

bool circlesOverlap(Vector2 centerA, float radiusA, Vector2 centerB, float radiusB) noexcept {
    const float combinedRadius = radiusA + radiusB;
    return core::distanceSquared(centerA, centerB) < combinedRadius * combinedRadius;
}

Vector2 pushOutOfCircle(Vector2 position, float radius, Vector2 obstaclePosition, float obstacleRadius) noexcept {
    const Vector2 offset = position - obstaclePosition;
    const float distance = offset.length();
    const float minimumDistance = radius + obstacleRadius;

    if (distance >= minimumDistance) {
        return position;
    }

    const Vector2 direction = (distance > 0.0f) ? offset * (1.0f / distance) : Vector2{1.0f, 0.0f};
    return obstaclePosition + direction * minimumDistance;
}

}  // namespace astroparty::physics
