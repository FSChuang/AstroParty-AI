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
const astroparty::core::SimulationConfig kConfig{};
const std::vector<ObstacleDefinition> kNoObstacles{};
}  // namespace

TEST_SUITE("ai::moveToward") {
    TEST_CASE("does not turn when already facing the target") {
        // Heading 0 degrees points toward -Y in this project's convention.
        Ship ship(0, Vector2{0.0f, 0.0f}, 0.0f);

        const ShipAction action = astroparty::ai::moveToward(ship, Vector2{0.0f, -500.0f}, kNoObstacles,
                                                               kConfig);

        CHECK_FALSE(action.turnRight);
        CHECK_FALSE(action.turnLeft);
        CHECK(action.throttle > 0.0f);
        CHECK_FALSE(action.shoot);
    }

    TEST_CASE("turns when not facing the target") {
        Ship ship(0, Vector2{0.0f, 0.0f}, 0.0f);

        const ShipAction action = astroparty::ai::moveToward(ship, Vector2{500.0f, 0.0f}, kNoObstacles,
                                                               kConfig);

        CHECK(action.turnRight);
        CHECK(action.throttle == 0.0f);
    }

    TEST_CASE("turns left when it is the shorter route to the target") {
        const Ship ship(0, Vector2{0.0f, 0.0f}, 10.0f);
        const ShipAction action = astroparty::ai::moveToward(ship, Vector2{-50.0f, -500.0f}, kNoObstacles, kConfig);

        CHECK(action.turnLeft);
        CHECK_FALSE(action.turnRight);
    }

    TEST_CASE("never shoots regardless of alignment") {
        Ship ship(0, Vector2{0.0f, 0.0f}, 180.0f);

        const ShipAction action = astroparty::ai::moveToward(ship, Vector2{0.0f, 500.0f}, kNoObstacles,
                                                               kConfig);

        CHECK_FALSE(action.shoot);
    }

    TEST_CASE("turns to avoid an obstacle directly ahead, overriding the target heading") {
        Ship ship(0, Vector2{0.0f, 0.0f}, 0.0f);  // Facing -Y, already aligned with the target below.
        const std::vector<ObstacleDefinition> obstacles{{Vector2{0.0f, -60.0f}, 20.0f}};

        const ShipAction action = astroparty::ai::moveToward(ship, Vector2{0.0f, -500.0f}, obstacles,
                                                               kConfig);

        CHECK(action.turnRight);
    }

    TEST_CASE("ignores an obstacle that is not in the way of the target heading") {
        Ship ship(0, Vector2{0.0f, 0.0f}, 0.0f);
        const std::vector<ObstacleDefinition> obstacles{{Vector2{1000.0f, 1000.0f}, 20.0f}};

        const ShipAction action = astroparty::ai::moveToward(ship, Vector2{0.0f, -500.0f}, obstacles,
                                                               kConfig);

        CHECK_FALSE(action.turnRight);
    }
}

TEST_SUITE("ai::moveWhileAvoidingEnemy") {
    TEST_CASE("sidesteps an enemy on its route while keeping pickup enabled") {
        const Ship collector(0, Vector2{500.0f, 350.0f}, 90.0f);
        const Ship enemy(1, Vector2{575.0f, 350.0f}, 270.0f);
        const auto action = astroparty::ai::moveWhileAvoidingEnemy(
            collector, Vector2{900.0f, 350.0f}, enemy, kNoObstacles, kConfig);

        CHECK(action.turnRight);
        CHECK_FALSE(action.shoot);
        CHECK(action.allowResourcePickup);
    }

    TEST_CASE("finishes a pickup already within reach") {
        const Ship collector(0, Vector2{500.0f, 350.0f}, 90.0f);
        const Ship enemy(1, Vector2{575.0f, 350.0f}, 270.0f);
        const auto action = astroparty::ai::moveWhileAvoidingEnemy(
            collector, Vector2{520.0f, 350.0f}, enemy, kNoObstacles, kConfig);

        CHECK_FALSE(action.turnLeft);
        CHECK_FALSE(action.turnRight);
        CHECK(action.throttle > 0.0f);
    }
}

TEST_SUITE("ai::pursueAndAttack") {
    TEST_CASE("shoots when aimed at and within range of the target") {
        Ship ship(0, Vector2{0.0f, 0.0f}, 0.0f);  // Facing -Y.

        const ShipAction action =
            astroparty::ai::pursueAndAttack(ship, Vector2{0.0f, -100.0f}, kNoObstacles, kConfig,
                                             /*engagementRange=*/300.0f);

        CHECK(action.shoot);
    }

    TEST_CASE("does not shoot when the target is out of engagement range") {
        Ship ship(0, Vector2{0.0f, 0.0f}, 0.0f);

        const ShipAction action =
            astroparty::ai::pursueAndAttack(ship, Vector2{0.0f, -1000.0f}, kNoObstacles, kConfig,
                                             /*engagementRange=*/300.0f);

        CHECK_FALSE(action.shoot);
    }

    TEST_CASE("does not shoot when not yet aimed at the target, but still turns toward it") {
        Ship ship(0, Vector2{0.0f, 0.0f}, 0.0f);  // Facing -Y; target is to the side.

        const ShipAction action =
            astroparty::ai::pursueAndAttack(ship, Vector2{100.0f, 0.0f}, kNoObstacles, kConfig,
                                             /*engagementRange=*/300.0f);

        CHECK_FALSE(action.shoot);
        CHECK(action.turnRight);
    }

    TEST_CASE("steers around an obstacle instead of firing into it") {
        Ship ship(0, Vector2{0.0f, 0.0f}, 0.0f);
        const std::vector<ObstacleDefinition> obstacles{{Vector2{0.0f, -60.0f}, 20.0f}};

        const ShipAction action =
            astroparty::ai::pursueAndAttack(ship, Vector2{0.0f, -100.0f}, obstacles, kConfig,
                                             /*engagementRange=*/300.0f);

        CHECK(action.turnRight);
        CHECK_FALSE(action.shoot);
    }

    TEST_CASE("leads a target moving across the firing lane") {
        const Ship ship(0, Vector2{0.0f, 0.0f}, 0.0f);
        const auto action = astroparty::ai::pursueAndAttack(ship, Vector2{0.0f, -200.0f},
            kNoObstacles, kConfig, 300.0f, Vector2{100.0f, 0.0f});

        CHECK(action.turnRight);
        CHECK_FALSE(action.shoot);
    }
}
