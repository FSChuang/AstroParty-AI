#include "astroparty/entities/Ship.hpp"

#include "astroparty/physics/Arena.hpp"
#include "astroparty/physics/Obstacle.hpp"

namespace astroparty::entities {

using core::SimulationConfig;
using core::Vector2;

Ship::Ship(int id, Vector2 spawnPosition, float spawnHeadingDegrees) noexcept
    : id_(id), position_(spawnPosition), headingDegrees_(spawnHeadingDegrees) {}

std::optional<BulletSpawnRequest> Ship::update(const core::ShipAction& action, float deltaSeconds,
                                                const SimulationConfig& config) noexcept {
    if (!isAlive_) {
        return std::nullopt;
    }

    if (action.turnRight) {
        headingDegrees_ += config.shipTurnRateDegreesPerSecond * deltaSeconds;
    }

    // The ship accelerates continuously along its current heading -- this is
    // the defining Astro Party mechanic: there is no "stop" control, only
    // steering. See docs/UPSTREAM_AUDIT.md,
    // section 4.
    const Vector2 forward = Vector2::fromAngleDegrees(headingDegrees_);
    velocity_ += forward * (config.shipAccelerationUnitsPerSecondSquared * deltaSeconds);
    velocity_ = velocity_.clampedToLength(config.shipMaxSpeedUnitsPerSecond);

    position_ += velocity_ * deltaSeconds;
    position_ = physics::clampToArena(position_, config.arenaSize, config.shipCollisionRadius);

    std::optional<BulletSpawnRequest> spawnRequest;
    if (action.shoot && !shootHeldPreviousStep_) {
        const Vector2 spawnPosition = position_ + forward * config.bulletSpawnOffset;
        spawnRequest = BulletSpawnRequest{spawnPosition, headingDegrees_};
    }
    shootHeldPreviousStep_ = action.shoot;

    return spawnRequest;
}

void Ship::eliminate() noexcept { isAlive_ = false; }

void Ship::resolveObstacleOverlap(Vector2 obstaclePosition, float obstacleRadius, float shipRadius) noexcept {
    if (!isAlive_) {
        return;
    }
    position_ = physics::pushOutOfCircle(position_, shipRadius, obstaclePosition, obstacleRadius);
}

void Ship::respawn(Vector2 position, float headingDegrees) noexcept {
    position_ = position;
    velocity_ = Vector2{0.0f, 0.0f};
    headingDegrees_ = headingDegrees;
    isAlive_ = true;
    shootHeldPreviousStep_ = false;
}

}  // namespace astroparty::entities
