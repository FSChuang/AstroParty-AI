#include "astroparty/core/Collisions.hpp"

#include "astroparty/physics/Collision.hpp"

namespace astroparty::core {

void resolveShipBulletCollisions(std::vector<entities::Ship>& ships, std::vector<entities::Bullet>& bullets,
                                  float shipCollisionRadius) noexcept {
    for (entities::Ship& ship : ships) {
        if (!ship.isAlive()) {
            continue;
        }
        for (entities::Bullet& bullet : bullets) {
            if (bullet.isExpired() || bullet.ownerShipId() == ship.id()) {
                continue;
            }
            if (physics::isPointWithinRadius(bullet.position(), ship.position(), shipCollisionRadius)) {
                ship.eliminate();
                bullet.expireImmediately();
                break;  // This ship is eliminated; no need to check its remaining bullets.
            }
        }
    }
}

void resolveShipObstacleCollisions(std::vector<entities::Ship>& ships, const std::vector<ObstacleDefinition>& obstacles,
                                    float shipCollisionRadius) noexcept {
    for (entities::Ship& ship : ships) {
        for (const ObstacleDefinition& obstacle : obstacles) {
            ship.resolveObstacleOverlap(obstacle.position, obstacle.radius, shipCollisionRadius);
        }
    }
}

void resolveBulletObstacleCollisions(std::vector<entities::Bullet>& bullets,
                                      const std::vector<ObstacleDefinition>& obstacles) noexcept {
    for (entities::Bullet& bullet : bullets) {
        if (bullet.isExpired()) {
            continue;
        }
        for (const ObstacleDefinition& obstacle : obstacles) {
            if (physics::isPointWithinRadius(bullet.position(), obstacle.position, obstacle.radius)) {
                bullet.expireImmediately();
                break;
            }
        }
    }
}

}  // namespace astroparty::core
