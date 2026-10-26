#include "astroparty/core/Collisions.hpp"

#include "astroparty/physics/Collision.hpp"
#include "astroparty/physics/PhysicsConstants.hpp"

namespace astroparty::core {

void resolveShipBulletCollisions(std::vector<entities::Ship>& ships,
                                  std::vector<entities::Bullet>& bullets) noexcept {
    for (entities::Ship& ship : ships) {
        if (!ship.isAlive()) {
            continue;
        }
        for (entities::Bullet& bullet : bullets) {
            if (bullet.isExpired() || bullet.ownerShipId() == ship.id()) {
                continue;
            }
            if (physics::isPointWithinRadius(bullet.position(), ship.position(), physics::kShipCollisionRadius)) {
                ship.eliminate();
                bullet.expireImmediately();
                break;  // This ship is eliminated; no need to check its remaining bullets.
            }
        }
    }
}

}  // namespace astroparty::core
