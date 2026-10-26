#pragma once

#include <optional>

#include "astroparty/core/Action.hpp"
#include "astroparty/core/Vector2.hpp"

namespace astroparty::entities {

// A request, produced by Ship::update, to create a new bullet.
//
// Ship has no knowledge of the Bullet type or of any entity container: it
// only reports that it fired and where, leaving World responsible for
// actually constructing and owning the resulting Bullet. This keeps entity
// creation policy out of the entity classes themselves.
struct BulletSpawnRequest {
    core::Vector2 position;
    float headingDegrees = 0.0f;
};

// A player- or AI-controlled spaceship.
//
// Ship owns only simulation state (position, velocity, heading, life
// status). It has no knowledge of SDL, rendering, or networking, so it can
// be constructed, updated, and asserted on from a plain unit test.
class Ship {
public:
    Ship(int id, core::Vector2 spawnPosition, float spawnHeadingDegrees) noexcept;

    // Advances this ship by deltaSeconds according to `action`, then clamps
    // its position to stay inside an arena of size `arenaSize`. Returns a
    // bullet spawn request if `action.shoot` transitioned from released to
    // pressed on this step: ships fire once per press rather than
    // continuously while held, matching upstream's
    // button_shoot / button_shoot_prev edge detection (see
    // astro-party-workspace/audit/03_code_architecture.md, section 3).
    //
    // Has no effect and returns std::nullopt once the ship has been
    // eliminated.
    std::optional<BulletSpawnRequest> update(const core::ShipAction& action, float deltaSeconds,
                                              core::Vector2 arenaSize) noexcept;

    // Marks this ship as eliminated. Idempotent.
    void eliminate() noexcept;

    [[nodiscard]] int id() const noexcept { return id_; }
    [[nodiscard]] core::Vector2 position() const noexcept { return position_; }
    [[nodiscard]] core::Vector2 velocity() const noexcept { return velocity_; }
    [[nodiscard]] float headingDegrees() const noexcept { return headingDegrees_; }
    [[nodiscard]] bool isAlive() const noexcept { return isAlive_; }

private:
    int id_;
    core::Vector2 position_;
    core::Vector2 velocity_{0.0f, 0.0f};
    float headingDegrees_;
    bool isAlive_ = true;

    // Remembers last step's shoot input so firing can be edge-detected.
    bool shootHeldPreviousStep_ = false;
};

}  // namespace astroparty::entities
