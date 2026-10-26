#include <doctest/doctest.h>

#include <vector>

#include "astroparty/core/MapDefinition.hpp"
#include "astroparty/core/MatchState.hpp"
#include "astroparty/core/SimulationConfig.hpp"
#include "astroparty/core/Vector2.hpp"
#include "astroparty/core/World.hpp"

using astroparty::core::MapDefinition;
using astroparty::core::MatchOutcome;
using astroparty::core::ShipAction;
using astroparty::core::ShipSpawnPoint;
using astroparty::core::SimulationConfig;
using astroparty::core::Vector2;
using astroparty::core::World;

// World's own responsibility is orchestration: advance ships, spawn bullets
// they request, delegate hit resolution, remove expired bullets, and
// evaluate the match outcome. The hit-resolution rule itself is exercised
// directly, with hand-placed entities, in test_collisions.cpp -- reproducing
// an exact bullet-ship impact here would require reverse-engineering the
// fixed spawn layout's geometry into a fragile, constant-dependent test
// without adding real coverage.
TEST_SUITE("World") {
    TEST_CASE("a freshly constructed world has two living ships, no bullets, and an undecided match") {
        World world;

        CHECK(world.ships().size() == 2);
        for (const auto& ship : world.ships()) {
            CHECK(ship.isAlive());
        }
        CHECK(world.bullets().empty());
        CHECK(world.matchState().outcome() == MatchOutcome::Undecided);
    }

    TEST_CASE("stepping with no input still advances both ships, since they accelerate continuously") {
        World world;
        const Vector2 initialPositionOfShip0 = world.ships()[0].position();
        const Vector2 initialPositionOfShip1 = world.ships()[1].position();

        const std::vector<ShipAction> noActions(2);
        for (int step = 0; step < 30; ++step) {
            world.step(noActions, 1.0f / 60.0f);
        }

        CHECK(world.ships()[0].position() != initialPositionOfShip0);
        CHECK(world.ships()[1].position() != initialPositionOfShip1);
    }

    TEST_CASE("firing spawns a bullet owned by the firing ship") {
        World world;
        std::vector<ShipAction> actions(2);
        actions[0].shoot = true;

        world.step(actions, 1.0f / 60.0f);

        REQUIRE(world.bullets().size() == 1);
        CHECK(world.bullets()[0].ownerShipId() == world.ships()[0].id());
    }

    TEST_CASE("a shorter action list is treated as no input for the missing ships") {
        World world;
        const std::vector<ShipAction> onlyFirstShipControlled(1);

        CHECK_NOTHROW(world.step(onlyFirstShipControlled, 1.0f / 60.0f));
    }

    TEST_CASE("reset clears bullets and restores two living ships with an undecided match") {
        World world;
        std::vector<ShipAction> bothShooting(2);
        bothShooting[0].shoot = true;
        bothShooting[1].shoot = true;
        world.step(bothShooting, 1.0f / 60.0f);
        REQUIRE_FALSE(world.bullets().empty());

        world.reset();

        CHECK(world.bullets().empty());
        CHECK(world.ships().size() == 2);
        for (const auto& ship : world.ships()) {
            CHECK(ship.isAlive());
        }
        CHECK(world.matchState().outcome() == MatchOutcome::Undecided);
    }

    TEST_CASE("a World built from a custom map spawns ships at the map's spawn points") {
        MapDefinition map;
        map.name = "three_ship_test_map";
        map.arenaSize = Vector2{640.0f, 480.0f};
        map.shipSpawns = {
            ShipSpawnPoint{Vector2{50.0f, 50.0f}, 0.0f},
            ShipSpawnPoint{Vector2{300.0f, 240.0f}, 90.0f},
            ShipSpawnPoint{Vector2{590.0f, 430.0f}, 180.0f},
        };

        World world(SimulationConfig{}, map);

        REQUIRE(world.ships().size() == 3);
        CHECK(world.ships()[0].position() == map.shipSpawns[0].position);
        CHECK(world.ships()[1].position() == map.shipSpawns[1].position);
        CHECK(world.ships()[2].position() == map.shipSpawns[2].position);
        CHECK(world.ships()[1].headingDegrees() == doctest::Approx(90.0f));
    }

    TEST_CASE("a World's arena size comes from the map, overriding the config's") {
        SimulationConfig config;
        config.arenaSize = Vector2{1280.0f, 720.0f};  // Deliberately different from the map below.

        MapDefinition map = MapDefinition::defaultTwoShipMap();
        map.arenaSize = Vector2{640.0f, 480.0f};

        World world(config, map);

        CHECK(world.arenaSize() == map.arenaSize);
        CHECK(world.config().arenaSize == map.arenaSize);
    }

    TEST_CASE("reset restores the configured map's spawn layout, not a hardcoded one") {
        MapDefinition map;
        map.name = "reset_test_map";
        map.arenaSize = Vector2{640.0f, 480.0f};
        map.shipSpawns = {
            ShipSpawnPoint{Vector2{10.0f, 10.0f}, 0.0f},
            ShipSpawnPoint{Vector2{600.0f, 400.0f}, 180.0f},
        };
        World world(SimulationConfig{}, map);

        const std::vector<ShipAction> noActions(2);
        for (int step = 0; step < 30; ++step) {
            world.step(noActions, 1.0f / 60.0f);
        }
        REQUIRE(world.ships()[0].position() != map.shipSpawns[0].position);

        world.reset();

        CHECK(world.ships()[0].position() == map.shipSpawns[0].position);
        CHECK(world.ships()[1].position() == map.shipSpawns[1].position);
    }
}
