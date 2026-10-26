#include <doctest/doctest.h>

#include <vector>

#include "astroparty/core/GameMode.hpp"
#include "astroparty/core/MapDefinition.hpp"
#include "astroparty/core/SimulationConfig.hpp"
#include "astroparty/core/TeamMatchState.hpp"
#include "astroparty/core/Vector2.hpp"
#include "astroparty/core/World.hpp"
#include "astroparty/entities/Resource.hpp"

using astroparty::core::GameMode;
using astroparty::core::HomeRegion;
using astroparty::core::MapDefinition;
using astroparty::core::ShipAction;
using astroparty::core::ShipSpawnPoint;
using astroparty::core::SimulationConfig;
using astroparty::core::TeamMatchOutcome;
using astroparty::core::Vector2;
using astroparty::core::World;
using astroparty::entities::Resource;
using astroparty::entities::ResourceState;

namespace {

constexpr float kFixedDeltaSeconds = 1.0f / 60.0f;

// A structurally valid 3v3 Team Resource Battle map, deliberately spacious
// (1000x1000) so individual tests can freely override spawn/home/resource
// positions for a specific scenario without the default layout interfering.
MapDefinition buildValidTeamMap() {
    MapDefinition map;
    map.name = "resource_test_map";
    map.arenaSize = Vector2{1000.0f, 1000.0f};
    map.shipSpawns = {
        ShipSpawnPoint{Vector2{50.0f, 50.0f}, 0.0f, 0},
        ShipSpawnPoint{Vector2{50.0f, 150.0f}, 0.0f, 0},
        ShipSpawnPoint{Vector2{50.0f, 250.0f}, 0.0f, 0},
        ShipSpawnPoint{Vector2{950.0f, 50.0f}, 180.0f, 1},
        ShipSpawnPoint{Vector2{950.0f, 150.0f}, 180.0f, 1},
        ShipSpawnPoint{Vector2{950.0f, 250.0f}, 180.0f, 1},
    };
    map.homeRegions = {
        HomeRegion{0, Vector2{20.0f, 150.0f}, 60.0f},
        HomeRegion{1, Vector2{980.0f, 150.0f}, 60.0f},
    };
    map.resourceSpawnPoints = {Vector2{500.0f, 500.0f}};
    return map;
}

std::vector<ShipAction> noActionsForSixShips() { return std::vector<ShipAction>(6); }

}  // namespace

TEST_SUITE("Resource") {
    TEST_CASE("a freshly constructed resource sits at its spawn point and is not carried") {
        Resource resource(0, Vector2{10.0f, 20.0f});

        CHECK(resource.state() == ResourceState::AtSpawn);
        CHECK_FALSE(resource.carrierShipId().has_value());
        CHECK(resource.spawnPosition() == Vector2{10.0f, 20.0f});
    }

    TEST_CASE("pickUp marks the resource as carried by the given ship") {
        Resource resource(0, Vector2{10.0f, 20.0f});
        resource.pickUp(3);

        CHECK(resource.state() == ResourceState::Carried);
        REQUIRE(resource.carrierShipId().has_value());
        CHECK(*resource.carrierShipId() == 3);
    }

    TEST_CASE("returnToSpawn clears the carrier and restores AtSpawn") {
        Resource resource(0, Vector2{10.0f, 20.0f});
        resource.pickUp(3);

        resource.returnToSpawn();

        CHECK(resource.state() == ResourceState::AtSpawn);
        CHECK_FALSE(resource.carrierShipId().has_value());
    }
}

TEST_SUITE("World Team Resource Battle setup") {
    TEST_CASE("a valid 3v3 map with home regions and a resource constructs successfully") {
        SimulationConfig config;
        config.mode = GameMode::TeamResourceBattle;

        CHECK_NOTHROW(World(config, buildValidTeamMap()));
    }

    TEST_CASE("construction throws if a team does not have exactly three ships") {
        SimulationConfig config;
        config.mode = GameMode::TeamResourceBattle;
        MapDefinition map = buildValidTeamMap();
        map.shipSpawns.pop_back();  // team 1 now has only two ships
        map.shipSpawns.push_back(ShipSpawnPoint{Vector2{500.0f, 500.0f}, 0.0f, 0});  // team 0 now has four

        CHECK_THROWS_AS((void)World(config, map), std::invalid_argument);
    }

    TEST_CASE("construction throws if any ship spawn lacks a team assignment") {
        SimulationConfig config;
        config.mode = GameMode::TeamResourceBattle;
        MapDefinition map = buildValidTeamMap();
        map.shipSpawns[0].teamId = -1;

        CHECK_THROWS_AS((void)World(config, map), std::invalid_argument);
    }

    TEST_CASE("construction throws if a team has no home region") {
        SimulationConfig config;
        config.mode = GameMode::TeamResourceBattle;
        MapDefinition map = buildValidTeamMap();
        map.homeRegions.pop_back();  // team 1 now has no home region

        CHECK_THROWS_AS((void)World(config, map), std::invalid_argument);
    }

    TEST_CASE("construction throws if the map has no resource spawn points") {
        SimulationConfig config;
        config.mode = GameMode::TeamResourceBattle;
        MapDefinition map = buildValidTeamMap();
        map.resourceSpawnPoints.clear();

        CHECK_THROWS_AS((void)World(config, map), std::invalid_argument);
    }

    TEST_CASE("a freshly constructed Team Resource Battle world has zero scores and one available resource") {
        SimulationConfig config;
        config.mode = GameMode::TeamResourceBattle;
        World world(config, buildValidTeamMap());

        CHECK(world.teamScores()[0] == 0);
        CHECK(world.teamScores()[1] == 0);
        REQUIRE(world.resources().size() == 1);
        CHECK(world.resources()[0].state() == ResourceState::AtSpawn);
        CHECK(world.teamMatchState().outcome() == TeamMatchOutcome::Undecided);
    }

    TEST_CASE("teamIdOfShip reports each ship's assigned team") {
        SimulationConfig config;
        config.mode = GameMode::TeamResourceBattle;
        World world(config, buildValidTeamMap());

        CHECK(world.teamIdOfShip(0) == 0);
        CHECK(world.teamIdOfShip(2) == 0);
        CHECK(world.teamIdOfShip(3) == 1);
        CHECK(world.teamIdOfShip(5) == 1);
    }

    TEST_CASE("teamIdOfShip is -1 for Classic Deathmatch's unassigned ships") {
        World world;  // Default Classic Deathmatch world.
        CHECK(world.teamIdOfShip(0) == -1);
        CHECK(world.teamIdOfShip(1) == -1);
    }
}

TEST_SUITE("World Team Resource Battle gameplay") {
    TEST_CASE("a ship spawned at a resource's position picks it up on the first step") {
        SimulationConfig config;
        config.mode = GameMode::TeamResourceBattle;
        MapDefinition map = buildValidTeamMap();
        map.shipSpawns[0].position = map.resourceSpawnPoints[0];  // ship 0 starts exactly on the resource
        World world(config, map);

        world.step(noActionsForSixShips(), kFixedDeltaSeconds);

        REQUIRE(world.resources()[0].state() == ResourceState::Carried);
        CHECK(*world.resources()[0].carrierShipId() == 0);
    }

    TEST_CASE("a ship already carrying a resource does not pick up a second one") {
        SimulationConfig config;
        config.mode = GameMode::TeamResourceBattle;
        MapDefinition map = buildValidTeamMap();
        map.shipSpawns[0].position = Vector2{500.0f, 500.0f};
        map.resourceSpawnPoints = {Vector2{500.0f, 500.0f}, Vector2{500.0f, 500.0f}};
        World world(config, map);

        world.step(noActionsForSixShips(), kFixedDeltaSeconds);

        const int carriedCount = (world.resources()[0].state() == ResourceState::Carried ? 1 : 0) +
                                  (world.resources()[1].state() == ResourceState::Carried ? 1 : 0);
        CHECK(carriedCount == 1);
    }

    TEST_CASE("delivering a resource to the carrier's own home region scores a point and resets the resource") {
        SimulationConfig config;
        config.mode = GameMode::TeamResourceBattle;
        config.winningScore = 100;  // High enough that this single delivery does not end the match.
        MapDefinition map = buildValidTeamMap();
        // Ship 0 (team 0) spawns simultaneously on the resource AND inside its own home region,
        // so pickup and delivery both complete within the same first step.
        const Vector2 spot{20.0f, 150.0f};
        map.shipSpawns[0].position = spot;
        map.resourceSpawnPoints = {spot};
        World world(config, map);

        world.step(noActionsForSixShips(), kFixedDeltaSeconds);

        CHECK(world.teamScores()[0] == 1);
        CHECK(world.teamScores()[1] == 0);
        CHECK(world.resources()[0].state() == ResourceState::AtSpawn);
    }

    TEST_CASE("delivering to the OTHER team's home region does not score") {
        SimulationConfig config;
        config.mode = GameMode::TeamResourceBattle;
        MapDefinition map = buildValidTeamMap();
        // Ship 0 is on team 0 but spawned inside team 1's home region.
        const Vector2 spot = map.homeRegions[1].position;
        map.shipSpawns[0].position = spot;
        map.resourceSpawnPoints = {spot};
        World world(config, map);

        world.step(noActionsForSixShips(), kFixedDeltaSeconds);
        REQUIRE(world.resources()[0].state() == ResourceState::Carried);  // picked up...

        for (int i = 0; i < 5; ++i) {
            world.step(noActionsForSixShips(), kFixedDeltaSeconds);
        }

        CHECK(world.teamScores()[0] == 0);
        CHECK(world.resources()[0].state() == ResourceState::Carried);  // ...but never delivered.
    }

    TEST_CASE("reaching the winning score ends the match as TeamWon") {
        SimulationConfig config;
        config.mode = GameMode::TeamResourceBattle;
        config.winningScore = 1;
        MapDefinition map = buildValidTeamMap();
        const Vector2 spot{20.0f, 150.0f};
        map.shipSpawns[0].position = spot;
        map.resourceSpawnPoints = {spot};
        World world(config, map);

        world.step(noActionsForSixShips(), kFixedDeltaSeconds);

        CHECK(world.teamMatchState().outcome() == TeamMatchOutcome::TeamWon);
        REQUIRE(world.teamMatchState().winningTeamId().has_value());
        CHECK(*world.teamMatchState().winningTeamId() == 0);
    }

    TEST_CASE("a resource carried by an eliminated ship is dropped back to its spawn point") {
        SimulationConfig config;
        config.mode = GameMode::TeamResourceBattle;
        MapDefinition map = buildValidTeamMap();
        map.shipSpawns[0] = ShipSpawnPoint{Vector2{500.0f, 500.0f}, 90.0f, 0};   // the carrier-to-be
        map.shipSpawns[3] = ShipSpawnPoint{Vector2{800.0f, 500.0f}, 270.0f, 1};  // the shooter, facing ship 0
        map.resourceSpawnPoints = {Vector2{500.0f, 500.0f}};
        World world(config, map);

        // Tick 1: ship 0 picks up the resource before anyone has fired.
        world.step(noActionsForSixShips(), kFixedDeltaSeconds);
        REQUIRE(world.resources()[0].state() == ResourceState::Carried);

        // Tick 2: the enemy fires a single shot toward ship 0.
        std::vector<ShipAction> enemyFires = noActionsForSixShips();
        enemyFires[3].shoot = true;
        world.step(enemyFires, kFixedDeltaSeconds);

        // Let the bullet travel until it lands (well within its 3-second lifetime).
        bool shipZeroDied = false;
        for (int i = 0; i < 150 && !shipZeroDied; ++i) {
            world.step(noActionsForSixShips(), kFixedDeltaSeconds);
            shipZeroDied = !world.ships()[0].isAlive();
        }

        REQUIRE(shipZeroDied);
        CHECK(world.resources()[0].state() == ResourceState::AtSpawn);
        CHECK_FALSE(world.resources()[0].carrierShipId().has_value());
    }

    TEST_CASE("an eliminated ship respawns at its original spawn point after the configured delay") {
        SimulationConfig config;
        config.mode = GameMode::TeamResourceBattle;
        config.respawnDelayTicks = 5;  // Short delay so the test does not need hundreds of steps.
        MapDefinition map = buildValidTeamMap();
        const Vector2 originalSpawn{500.0f, 500.0f};
        map.shipSpawns[0] = ShipSpawnPoint{originalSpawn, 90.0f, 0};
        map.shipSpawns[3] = ShipSpawnPoint{Vector2{800.0f, 500.0f}, 270.0f, 1};
        World world(config, map);

        std::vector<ShipAction> enemyFires = noActionsForSixShips();
        enemyFires[3].shoot = true;
        world.step(enemyFires, kFixedDeltaSeconds);

        bool shipZeroDied = false;
        for (int i = 0; i < 150 && !shipZeroDied; ++i) {
            world.step(noActionsForSixShips(), kFixedDeltaSeconds);
            shipZeroDied = !world.ships()[0].isAlive();
        }
        REQUIRE(shipZeroDied);

        // Step exactly until the ship is alive again, and check its position
        // on that very tick -- one step too many would let it start
        // accelerating away from its freshly respawned position.
        bool shipZeroRespawned = false;
        for (int i = 0; i < config.respawnDelayTicks + 1 && !shipZeroRespawned; ++i) {
            world.step(noActionsForSixShips(), kFixedDeltaSeconds);
            shipZeroRespawned = world.ships()[0].isAlive();
        }

        REQUIRE(shipZeroRespawned);
        CHECK(world.ships()[0].position() == originalSpawn);
    }

    TEST_CASE("resourcesCollected increments on pickup, independent of whether it is later delivered") {
        SimulationConfig config;
        config.mode = GameMode::TeamResourceBattle;
        MapDefinition map = buildValidTeamMap();
        map.shipSpawns[0].position = Vector2{500.0f, 500.0f};
        map.resourceSpawnPoints = {Vector2{500.0f, 500.0f}};
        World world(config, map);

        world.step(noActionsForSixShips(), kFixedDeltaSeconds);

        CHECK(world.resourcesCollected()[0] == 1);
        CHECK(world.resourcesCollected()[1] == 0);
        CHECK(world.teamScores()[0] == 0);  // Picked up, but not yet delivered.
    }

    TEST_CASE("shipsDestroyed counts each elimination exactly once, even across many ticks while dead") {
        SimulationConfig config;
        config.mode = GameMode::TeamResourceBattle;
        MapDefinition map = buildValidTeamMap();
        map.shipSpawns[0] = ShipSpawnPoint{Vector2{500.0f, 500.0f}, 90.0f, 0};
        map.shipSpawns[3] = ShipSpawnPoint{Vector2{800.0f, 500.0f}, 270.0f, 1};
        World world(config, map);

        std::vector<ShipAction> enemyFires = noActionsForSixShips();
        enemyFires[3].shoot = true;
        world.step(enemyFires, kFixedDeltaSeconds);

        bool shipZeroDied = false;
        for (int i = 0; i < 150 && !shipZeroDied; ++i) {
            world.step(noActionsForSixShips(), kFixedDeltaSeconds);
            shipZeroDied = !world.ships()[0].isAlive();
        }
        REQUIRE(shipZeroDied);

        // Keep stepping well past the moment of death, while still dead
        // (respawnDelayTicks defaults to 180): the count must not keep
        // incrementing just because the ship remains dead for many ticks.
        for (int i = 0; i < 20; ++i) {
            world.step(noActionsForSixShips(), kFixedDeltaSeconds);
        }

        CHECK(world.shipsDestroyed()[0] == 1);
        CHECK(world.shipsDestroyed()[1] == 0);
    }

    TEST_CASE("reset clears scores, resources, and respawn state") {
        SimulationConfig config;
        config.mode = GameMode::TeamResourceBattle;
        config.winningScore = 1;
        MapDefinition map = buildValidTeamMap();
        const Vector2 spot{20.0f, 150.0f};
        map.shipSpawns[0].position = spot;
        map.resourceSpawnPoints = {spot};
        World world(config, map);

        world.step(noActionsForSixShips(), kFixedDeltaSeconds);
        REQUIRE(world.teamScores()[0] == 1);
        REQUIRE(world.teamMatchState().hasEnded());

        world.reset();

        CHECK(world.teamScores()[0] == 0);
        CHECK(world.teamScores()[1] == 0);
        CHECK(world.resources()[0].state() == ResourceState::AtSpawn);
        CHECK_FALSE(world.teamMatchState().hasEnded());
        CHECK(world.ships()[0].position() == spot);  // Back to its map spawn point.
    }
}
