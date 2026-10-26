#include "astroparty/entities/Bullet.hpp"

namespace astroparty::entities {

using core::SimulationConfig;
using core::Vector2;

Bullet::Bullet(int ownerShipId, Vector2 spawnPosition, float headingDegrees, const SimulationConfig& config) noexcept
    : ownerShipId_(ownerShipId),
      position_(spawnPosition),
      velocity_(Vector2::fromAngleDegrees(headingDegrees) * config.bulletSpeedUnitsPerSecond),
      remainingLifetimeSeconds_(config.bulletLifetimeSeconds) {}

void Bullet::update(float deltaSeconds) noexcept {
    if (isExpired()) {
        return;
    }
    position_ += velocity_ * deltaSeconds;
    remainingLifetimeSeconds_ -= deltaSeconds;
}

}  // namespace astroparty::entities
