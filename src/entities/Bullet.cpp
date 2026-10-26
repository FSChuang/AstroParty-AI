#include "astroparty/entities/Bullet.hpp"

#include "astroparty/physics/PhysicsConstants.hpp"

namespace astroparty::entities {

using core::Vector2;

Bullet::Bullet(int ownerShipId, Vector2 spawnPosition, float headingDegrees) noexcept
    : ownerShipId_(ownerShipId),
      position_(spawnPosition),
      velocity_(Vector2::fromAngleDegrees(headingDegrees) * physics::kBulletSpeedUnitsPerSecond),
      remainingLifetimeSeconds_(physics::kBulletLifetimeSeconds) {}

void Bullet::update(float deltaSeconds) noexcept {
    if (isExpired()) {
        return;
    }
    position_ += velocity_ * deltaSeconds;
    remainingLifetimeSeconds_ -= deltaSeconds;
}

}  // namespace astroparty::entities
