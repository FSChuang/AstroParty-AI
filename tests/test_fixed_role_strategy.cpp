#include <doctest/doctest.h>

#include <vector>

#include "astroparty/ai/FixedRoleTeamStrategy.hpp"
#include "astroparty/core/GameMode.hpp"
#include "astroparty/core/MapDefinition.hpp"
#include "astroparty/core/SimulationConfig.hpp"
#include "astroparty/core/Vector2.hpp"
#include "astroparty/core/World.hpp"

using astroparty::ai::FixedRoleTeamStrategy;
using astroparty::ai::ShipGoal;
using astroparty::ai::ShipRole;
using astroparty::core::GameMode;
using astroparty::core::HomeRegion;
using astroparty::core::MapDefinition;
using astroparty::core::ShipAction;
using astroparty::core::ShipSpawnPoint;
using astroparty::core::SimulationConfig;
using astroparty::core::Vector2;
using astroparty::core::World;

namespace {

constexpr float kFixedDeltaSeconds = 1.0f / 60.0f;

MapDefinition buildTeamMap() {
    MapDefinition map;
    map.name = "fixed_role_test_map";
    map.arenaSize = Vector2{1000.0f, 1000.0f};
    map.shipSpawns = {
        ShipSpawnPoint{Vector2{50.0f, 50.0f}, 0.0f, 0},   ShipSpawnPoint{Vector2{50.0f, 150.0f}, 0.0f, 0},
        ShipSpawnPoint{Vector2{50.0f, 450.0f}, 0.0f, 0},  ShipSpawnPoint{Vector2{950.0f, 50.0f}, 180.0f, 1},
        ShipSpawnPoint{Vector2{950.0f, 150.0f}, 180.0f, 1}, ShipSpawnPoint{Vector2{950.0f, 450.0f}, 180.0f, 1},
    };
    map.homeRegions = {HomeRegion{0, Vector2{20.0f, 150.0f}, 60.0f}, HomeRegion{1, Vector2{980.0f, 150.0f}, 60.0f}};
    map.resourceSpawnPoints = {Vector2{500.0f, 500.0f}};
    return map;
}

std::vector<ShipAction> noActionsForSixShips() { return std::vector<ShipAction>(6); }
const std::vector<int> kTeam0Ships{0, 1, 2};

}  // namespace

TEST_SUITE("FixedRoleTeamStrategy") {
    TEST_CASE("assigns Collector, Defender, Interceptor by fixed ship position") {
        SimulationConfig config;
        config.mode = GameMode::TeamResourceBattle;
        World world(config, buildTeamMap());
        FixedRoleTeamStrategy strategy;

        const std::vector<ShipGoal> goals = strategy.assignGoals(world, 0, kTeam0Ships);

        REQUIRE(goals.size() == 3);
        CHECK(goals[0].role == ShipRole::Collector);
        CHECK(goals[1].role == ShipRole::Defender);
        CHECK(goals[2].role == ShipRole::Interceptor);
    }

    TEST_CASE("roles never change across ticks when nothing relevant happens") {
        SimulationConfig config;
        config.mode = GameMode::TeamResourceBattle;
        World world(config, buildTeamMap());
        FixedRoleTeamStrategy strategy;

        for (int i = 0; i < 120; ++i) {
            const std::vector<ShipGoal> goals = strategy.assignGoals(world, 0, kTeam0Ships);
            CHECK(goals[0].role == ShipRole::Collector);
            CHECK(goals[1].role == ShipRole::Defender);
            CHECK(goals[2].role == ShipRole::Interceptor);
            world.step(noActionsForSixShips(), kFixedDeltaSeconds);
        }
    }

    TEST_CASE("the Interceptor always targets the nearest enemy ship, never patrolling") {
        SimulationConfig config;
        config.mode = GameMode::TeamResourceBattle;
        World world(config, buildTeamMap());
        FixedRoleTeamStrategy strategy;

        const std::vector<ShipGoal> goals = strategy.assignGoals(world, 0, kTeam0Ships);

        REQUIRE(goals[2].targetShipId.has_value());
        CHECK(world.teamIdOfShip(*goals[2].targetShipId) == 1);
    }

    TEST_CASE("a carrying ship overrides its fixed role to Collector-deliver") {
        SimulationConfig config;
        config.mode = GameMode::TeamResourceBattle;
        MapDefinition map = buildTeamMap();
        map.shipSpawns[1].position = map.resourceSpawnPoints[0];  // The fixed Defender starts on the resource.
        World world(config, map);
        world.step(noActionsForSixShips(), kFixedDeltaSeconds);  // It picks it up.

        FixedRoleTeamStrategy strategy;
        const std::vector<ShipGoal> goals = strategy.assignGoals(world, 0, kTeam0Ships);

        CHECK(goals[1].role == ShipRole::Collector);
        CHECK(goals[1].targetPosition == world.map().homeRegions[0].position);
    }

    TEST_CASE("an eliminated ship gets a harmless placeholder goal, not its fixed role") {
        SimulationConfig config;
        config.mode = GameMode::TeamResourceBattle;
        MapDefinition map = buildTeamMap();
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

        FixedRoleTeamStrategy strategy;
        const std::vector<ShipGoal> goals = strategy.assignGoals(world, 0, kTeam0Ships);

        CHECK(goals[0].role == ShipRole::Defender);
        CHECK_FALSE(goals[0].targetShipId.has_value());
    }
}
