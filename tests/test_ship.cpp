#include <doctest/doctest.h>

#include "astroparty/core/Vector2.hpp"
#include "astroparty/entities/Ship.hpp"
#include "astroparty/physics/PhysicsConstants.hpp"

using astroparty::core::ShipAction;
using astroparty::core::Vector2;
using astroparty::entities::Ship;
namespace physics = astroparty::physics;

namespace {
constexpr float kFixedDeltaSeconds = 1.0f / 60.0f;
}

TEST_SUITE("Ship") {
    TEST_CASE("a ship accelerates continuously even with no input") {
        Ship ship(0, Vector2{640.0f, 360.0f}, 0.0f);
        const Vector2 startPosition = ship.position();

        for (int step = 0; step < 10; ++step) {
            ship.update(ShipAction{}, kFixedDeltaSeconds, physics::kArenaSize);
        }

        CHECK(ship.position() != startPosition);
        CHECK(ship.velocity().length() > 0.0f);
    }

    TEST_CASE("turnRight increases heading at the configured turn rate") {
        Ship ship(0, Vector2{100.0f, 100.0f}, 0.0f);
        ShipAction turning;
        turning.turnRight = true;

        ship.update(turning, kFixedDeltaSeconds, physics::kArenaSize);

        CHECK(ship.headingDegrees() == doctest::Approx(physics::kShipTurnRateDegreesPerSecond * kFixedDeltaSeconds));
    }

    TEST_CASE("ship speed never exceeds the configured maximum") {
        Ship ship(0, Vector2{640.0f, 360.0f}, 0.0f);
        for (int step = 0; step < 600; ++step) {
            ship.update(ShipAction{}, kFixedDeltaSeconds, physics::kArenaSize);
            CHECK(ship.velocity().length() <= physics::kShipMaxSpeedUnitsPerSecond + 0.01f);
        }
    }

    TEST_CASE("ship position is clamped within the arena bounds") {
        Ship ship(0, Vector2{50.0f, 50.0f}, 90.0f);  // heading toward +X
        const Vector2 smallArena{100.0f, 100.0f};

        for (int step = 0; step < 1000; ++step) {
            ship.update(ShipAction{}, kFixedDeltaSeconds, smallArena);
        }

        CHECK(ship.position().x <= smallArena.x - physics::kShipCollisionRadius + 0.01f);
        CHECK(ship.position().x >= physics::kShipCollisionRadius - 0.01f);
        CHECK(ship.position().y <= smallArena.y - physics::kShipCollisionRadius + 0.01f);
        CHECK(ship.position().y >= physics::kShipCollisionRadius - 0.01f);
    }

    TEST_CASE("a ship fires once per button press, not continuously while held") {
        Ship ship(0, Vector2{100.0f, 100.0f}, 0.0f);
        ShipAction shoot;
        shoot.shoot = true;

        const auto firstPress = ship.update(shoot, kFixedDeltaSeconds, physics::kArenaSize);
        const auto heldSecondStep = ship.update(shoot, kFixedDeltaSeconds, physics::kArenaSize);

        CHECK(firstPress.has_value());
        CHECK_FALSE(heldSecondStep.has_value());
    }

    TEST_CASE("a ship fires again after releasing and re-pressing shoot") {
        Ship ship(0, Vector2{100.0f, 100.0f}, 0.0f);
        ShipAction shoot;
        shoot.shoot = true;
        const ShipAction release{};

        CHECK(ship.update(shoot, kFixedDeltaSeconds, physics::kArenaSize).has_value());
        CHECK_FALSE(ship.update(shoot, kFixedDeltaSeconds, physics::kArenaSize).has_value());
        CHECK_FALSE(ship.update(release, kFixedDeltaSeconds, physics::kArenaSize).has_value());
        CHECK(ship.update(shoot, kFixedDeltaSeconds, physics::kArenaSize).has_value());
    }

    TEST_CASE("a fired bullet spawns ahead of the ship along its current heading") {
        Ship ship(0, Vector2{100.0f, 100.0f}, 0.0f);  // heading up (-Y)
        ShipAction shoot;
        shoot.shoot = true;

        const auto spawnRequest = ship.update(shoot, kFixedDeltaSeconds, physics::kArenaSize);

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
        const auto spawnRequest = ship.update(shoot, kFixedDeltaSeconds, physics::kArenaSize);

        CHECK_FALSE(spawnRequest.has_value());
        CHECK(ship.position() == positionBeforeUpdate);
        CHECK_FALSE(ship.isAlive());
    }
}
