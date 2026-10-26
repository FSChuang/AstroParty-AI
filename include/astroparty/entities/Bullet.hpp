#pragma once

#include "astroparty/core/SimulationConfig.hpp"
#include "astroparty/core/Vector2.hpp"

namespace astroparty::entities {

// A projectile fired by a ship. Like Ship, Bullet is pure simulation state
// with no rendering or SDL dependency.
class Bullet {
public:
    Bullet(int ownerShipId, core::Vector2 spawnPosition, float headingDegrees,
           const core::SimulationConfig& config) noexcept;

    // Moves the bullet in a straight line and counts down its remaining
    // lifetime. Has no effect once the bullet has already expired.
    void update(float deltaSeconds) noexcept;

    // True once the bullet has traveled for its full lifetime or has hit a
    // ship (see expireImmediately()). World removes expired bullets at the
    // end of each step.
    [[nodiscard]] bool isExpired() const noexcept { return remainingLifetimeSeconds_ <= 0.0f; }

    // Ends this bullet's life immediately, e.g. because it just hit a ship.
    // Reuses the same expiry mechanism as running out of lifetime, so World
    // only needs one removal path.
    void expireImmediately() noexcept { remainingLifetimeSeconds_ = 0.0f; }

    [[nodiscard]] int ownerShipId() const noexcept { return ownerShipId_; }
    [[nodiscard]] core::Vector2 position() const noexcept { return position_; }

private:
    int ownerShipId_;
    core::Vector2 position_;
    core::Vector2 velocity_;
    float remainingLifetimeSeconds_;
};

}  // namespace astroparty::entities
