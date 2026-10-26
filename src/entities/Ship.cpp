#include "astroparty/entities/Ship.hpp"

#include "astroparty/physics/Arena.hpp"
#include "astroparty/physics/PhysicsConstants.hpp"

namespace astroparty::entities {

using core::Vector2;

Ship::Ship(int id, Vector2 spawnPosition, float spawnHeadingDegrees) noexcept
    : id_(id), position_(spawnPosition), headingDegrees_(spawnHeadingDegrees) {}

std::optional<BulletSpawnRequest> Ship::update(const core::ShipAction& action, float deltaSeconds,
                                                Vector2 arenaSize) noexcept {
    if (!isAlive_) {
        return std::nullopt;
    }

    if (action.turnRight) {
        headingDegrees_ += physics::kShipTurnRateDegreesPerSecond * deltaSeconds;
    }

    // The ship accelerates continuously along its current heading -- this is
    // the defining Astro Party mechanic: there is no "stop" control, only
    // steering. See astro-party-workspace/audit/03_code_architecture.md,
    // section 4.
    const Vector2 forward = Vector2::fromAngleDegrees(headingDegrees_);
    velocity_ += forward * (physics::kShipAccelerationUnitsPerSecondSquared * deltaSeconds);
    velocity_ = velocity_.clampedToLength(physics::kShipMaxSpeedUnitsPerSecond);

    position_ += velocity_ * deltaSeconds;
    position_ = physics::clampToArena(position_, arenaSize, physics::kShipCollisionRadius);

    std::optional<BulletSpawnRequest> spawnRequest;
    if (action.shoot && !shootHeldPreviousStep_) {
        const Vector2 spawnPosition = position_ + forward * physics::kBulletSpawnOffset;
        spawnRequest = BulletSpawnRequest{spawnPosition, headingDegrees_};
    }
    shootHeldPreviousStep_ = action.shoot;

    return spawnRequest;
}

void Ship::eliminate() noexcept { isAlive_ = false; }

}  // namespace astroparty::entities
