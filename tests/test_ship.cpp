#include <doctest/doctest.h>

#include "astroparty/core/SimulationConfig.hpp"
#include "astroparty/core/Vector2.hpp"
#include "astroparty/entities/Ship.hpp"

using astroparty::core::ShipAction;
using astroparty::core::SimulationConfig;
using astroparty::core::Vector2;
using astroparty::entities::Ship;

namespace {
constexpr float kFixedDeltaSeconds = 1.0f / 60.0f;
const SimulationConfig kDefaultConfig{};  // Milestone-1-compatible defaults.
}

TEST_SUITE("Ship") {
    TEST_CASE("a ship accelerates continuously even with no input") {
        Ship ship(0, Vector2{640.0f, 360.0f}, 0.0f);
        const Vector2 startPosition = ship.position();

        for (int step = 0; step < 10; ++step) {
            ship.update(ShipAction{}, kFixedDeltaSeconds, kDefaultConfig);
        }

        CHECK(ship.position() != startPosition);
        CHECK(ship.velocity().length() > 0.0f);
    }

    TEST_CASE("turnRight increases heading at the configured turn rate") {
        Ship ship(0, Vector2{100.0f, 100.0f}, 0.0f);
        ShipAction turning;
        turning.turnRight = true;

        ship.update(turning, kFixedDeltaSeconds, kDefaultConfig);

        CHECK(ship.headingDegrees() ==
              doctest::Approx(kDefaultConfig.shipTurnRateDegreesPerSecond * kFixedDeltaSeconds));
    }

    TEST_CASE("ship speed never exceeds the configured maximum") {
        Ship ship(0, Vector2{640.0f, 360.0f}, 0.0f);
        for (int step = 0; step < 600; ++step) {
            ship.update(ShipAction{}, kFixedDeltaSeconds, kDefaultConfig);
            CHECK(ship.velocity().length() <= kDefaultConfig.shipMaxSpeedUnitsPerSecond + 0.01f);
        }
    }

    TEST_CASE("a higher configured acceleration changes the ship's trajectory") {
        // Confirms Ship actually reads its parameters from `config` rather
        // than from any remaining hardcoded constant.
        SimulationConfig fastConfig = kDefaultConfig;
        fastConfig.shipAccelerationUnitsPerSecondSquared *= 4.0f;

        Ship defaultShip(0, Vector2{640.0f, 360.0f}, 0.0f);
        Ship fastShip(0, Vector2{640.0f, 360.0f}, 0.0f);
        defaultShip.update(ShipAction{}, kFixedDeltaSeconds, kDefaultConfig);
        fastShip.update(ShipAction{}, kFixedDeltaSeconds, fastConfig);

        CHECK(fastShip.velocity().length() > defaultShip.velocity().length());
    }

    TEST_CASE("ship position is clamped within the arena bounds") {
        Ship ship(0, Vector2{50.0f, 50.0f}, 90.0f);  // heading toward +X
        SimulationConfig smallArenaConfig = kDefaultConfig;
        smallArenaConfig.arenaSize = Vector2{100.0f, 100.0f};

        for (int step = 0; step < 1000; ++step) {
            ship.update(ShipAction{}, kFixedDeltaSeconds, smallArenaConfig);
        }

        const float radius = smallArenaConfig.shipCollisionRadius;
        CHECK(ship.position().x <= smallArenaConfig.arenaSize.x - radius + 0.01f);
        CHECK(ship.position().x >= radius - 0.01f);
        CHECK(ship.position().y <= smallArenaConfig.arenaSize.y - radius + 0.01f);
        CHECK(ship.position().y >= radius - 0.01f);
    }

    TEST_CASE("a ship fires once per button press, not continuously while held") {
        Ship ship(0, Vector2{100.0f, 100.0f}, 0.0f);
        ShipAction shoot;
        shoot.shoot = true;

        const auto firstPress = ship.update(shoot, kFixedDeltaSeconds, kDefaultConfig);
        const auto heldSecondStep = ship.update(shoot, kFixedDeltaSeconds, kDefaultConfig);

        CHECK(firstPress.has_value());
        CHECK_FALSE(heldSecondStep.has_value());
    }

    TEST_CASE("a ship fires again after releasing and re-pressing shoot") {
        Ship ship(0, Vector2{100.0f, 100.0f}, 0.0f);
        ShipAction shoot;
        shoot.shoot = true;
        const ShipAction release{};

        CHECK(ship.update(shoot, kFixedDeltaSeconds, kDefaultConfig).has_value());
        CHECK_FALSE(ship.update(shoot, kFixedDeltaSeconds, kDefaultConfig).has_value());
        CHECK_FALSE(ship.update(release, kFixedDeltaSeconds, kDefaultConfig).has_value());
        CHECK(ship.update(shoot, kFixedDeltaSeconds, kDefaultConfig).has_value());
    }

    TEST_CASE("a fired bullet spawns ahead of the ship along its current heading") {
        Ship ship(0, Vector2{100.0f, 100.0f}, 0.0f);  // heading up (-Y)
        ShipAction shoot;
        shoot.shoot = true;

        const auto spawnRequest = ship.update(shoot, kFixedDeltaSeconds, kDefaultConfig);

        REQUIRE(spawnRequest.has_value());
        CHECK(spawnRequest->position.y < ship.position().y);
        CHECK(spawnRequest->headingDegrees == doctest::Approx(ship.headingDegrees()));
    }

    TEST_CASE("update has no effect once the ship has been eliminated") {
        Ship ship(0, Vector2{100.0f, 100.0f}, 0.0f);
        ship.eliminate();
        const Vector2 positionBeforeUpdate = ship.position();

        ShipAction shoot;
        shoot.shoot = true;
        const auto spawnRequest = ship.update(shoot, kFixedDeltaSeconds, kDefaultConfig);

        CHECK_FALSE(spawnRequest.has_value());
        CHECK(ship.position() == positionBeforeUpdate);
        CHECK_FALSE(ship.isAlive());
    }
}
