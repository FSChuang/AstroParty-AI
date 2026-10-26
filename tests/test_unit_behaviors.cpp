#include <doctest/doctest.h>

#include <vector>

#include "astroparty/ai/UnitBehaviors.hpp"
#include "astroparty/core/MapDefinition.hpp"
#include "astroparty/core/Vector2.hpp"
#include "astroparty/entities/Ship.hpp"

using astroparty::core::ObstacleDefinition;
using astroparty::core::ShipAction;
using astroparty::core::Vector2;
using astroparty::entities::Ship;

namespace {
constexpr float kShipCollisionRadius = 12.0f;
const std::vector<ObstacleDefinition> kNoObstacles{};
}  // namespace

TEST_SUITE("ai::moveToward") {
    TEST_CASE("does not turn when already facing the target") {
        // Heading 0 degrees points toward -Y in this project's convention.
        Ship ship(0, Vector2{0.0f, 0.0f}, 0.0f);

        const ShipAction action = astroparty::ai::moveToward(ship, Vector2{0.0f, -500.0f}, kNoObstacles,
                                                               kShipCollisionRadius);

        CHECK_FALSE(action.turnRight);
        CHECK_FALSE(action.shoot);
    }

    TEST_CASE("turns when not facing the target") {
        Ship ship(0, Vector2{0.0f, 0.0f}, 0.0f);

        const ShipAction action = astroparty::ai::moveToward(ship, Vector2{500.0f, 0.0f}, kNoObstacles,
                                                               kShipCollisionRadius);

        CHECK(action.turnRight);
    }

    TEST_CASE("never shoots regardless of alignment") {
        Ship ship(0, Vector2{0.0f, 0.0f}, 180.0f);

        const ShipAction action = astroparty::ai::moveToward(ship, Vector2{0.0f, 500.0f}, kNoObstacles,
                                                               kShipCollisionRadius);

        CHECK_FALSE(action.shoot);
    }

    TEST_CASE("turns to avoid an obstacle directly ahead, overriding the target heading") {
        Ship ship(0, Vector2{0.0f, 0.0f}, 0.0f);  // Facing -Y, already aligned with the target below.
        const std::vector<ObstacleDefinition> obstacles{{Vector2{0.0f, -60.0f}, 20.0f}};

        const ShipAction action = astroparty::ai::moveToward(ship, Vector2{0.0f, -500.0f}, obstacles,
                                                               kShipCollisionRadius);

        CHECK(action.turnRight);
    }

    TEST_CASE("ignores an obstacle that is not in the way of the target heading") {
        Ship ship(0, Vector2{0.0f, 0.0f}, 0.0f);
        const std::vector<ObstacleDefinition> obstacles{{Vector2{1000.0f, 1000.0f}, 20.0f}};

        const ShipAction action = astroparty::ai::moveToward(ship, Vector2{0.0f, -500.0f}, obstacles,
                                                               kShipCollisionRadius);

        CHECK_FALSE(action.turnRight);
    }
}

TEST_SUITE("ai::pursueAndAttack") {
    TEST_CASE("shoots when aimed at and within range of the target") {
        Ship ship(0, Vector2{0.0f, 0.0f}, 0.0f);  // Facing -Y.

        const ShipAction action =
            astroparty::ai::pursueAndAttack(ship, Vector2{0.0f, -100.0f}, kNoObstacles, kShipCollisionRadius,
                                             /*engagementRange=*/300.0f);

        CHECK(action.shoot);
    }

    TEST_CASE("does not shoot when the target is out of engagement range") {
        Ship ship(0, Vector2{0.0f, 0.0f}, 0.0f);

        const ShipAction action =
            astroparty::ai::pursueAndAttack(ship, Vector2{0.0f, -1000.0f}, kNoObstacles, kShipCollisionRadius,
                                             /*engagementRange=*/300.0f);

        CHECK_FALSE(action.shoot);
    }

    TEST_CASE("does not shoot when not yet aimed at the target, but still turns toward it") {
        Ship ship(0, Vector2{0.0f, 0.0f}, 0.0f);  // Facing -Y; target is to the side.

        const ShipAction action =
            astroparty::ai::pursueAndAttack(ship, Vector2{100.0f, 0.0f}, kNoObstacles, kShipCollisionRadius,
                                             /*engagementRange=*/300.0f);

        CHECK_FALSE(action.shoot);
        CHECK(action.turnRight);
    }

    TEST_CASE("still steers around an obstacle ahead even while aimed at and in range of its target") {
        // Obstacle-vs-bullet collision is handled separately by World's
        // resolveBulletObstacleCollisions, so pursueAndAttack does not need
        // to (and deliberately does not) suppress firing just because it is
        // also avoiding an obstacle -- the two concerns are independent.
        Ship ship(0, Vector2{0.0f, 0.0f}, 0.0f);
        const std::vector<ObstacleDefinition> obstacles{{Vector2{0.0f, -60.0f}, 20.0f}};

        const ShipAction action =
            astroparty::ai::pursueAndAttack(ship, Vector2{0.0f, -100.0f}, obstacles, kShipCollisionRadius,
                                             /*engagementRange=*/300.0f);

        CHECK(action.turnRight);
        CHECK(action.shoot);
    }
}
