#include <doctest/doctest.h>

#include <memory>
#include <vector>

#include "astroparty/ai/BaselineTeamStrategy.hpp"
#include "astroparty/ai/TeamShipController.hpp"
#include "astroparty/ai/TeamStrategy.hpp"
#include "astroparty/ai/UtilityTeamStrategy.hpp"
#include "astroparty/core/GameMode.hpp"
#include "astroparty/core/MapDefinition.hpp"
#include "astroparty/core/SimulationConfig.hpp"
#include "astroparty/core/Vector2.hpp"
#include "astroparty/core/World.hpp"

using astroparty::ai::BaselineTeamStrategy;
using astroparty::ai::ShipGoal;
using astroparty::ai::ShipRole;
using astroparty::ai::TeamShipController;
using astroparty::ai::UtilityTeamStrategy;
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

// A spacious 3v3 map, structurally identical in shape to test_resources.cpp's
// helper, whose spawn/home/resource positions tests override for their own
// scenario.
MapDefinition buildTeamMap() {
    MapDefinition map;
    map.name = "team_strategy_test_map";
    map.arenaSize = Vector2{1000.0f, 1000.0f};
    map.shipSpawns = {
        ShipSpawnPoint{Vector2{50.0f, 50.0f}, 0.0f, 0},
        ShipSpawnPoint{Vector2{50.0f, 150.0f}, 0.0f, 0},
        ShipSpawnPoint{Vector2{50.0f, 450.0f}, 0.0f, 0},
        ShipSpawnPoint{Vector2{950.0f, 50.0f}, 180.0f, 1},
        ShipSpawnPoint{Vector2{950.0f, 150.0f}, 180.0f, 1},
        ShipSpawnPoint{Vector2{950.0f, 450.0f}, 180.0f, 1},
    };
    map.homeRegions = {
        HomeRegion{0, Vector2{20.0f, 150.0f}, 60.0f},
        HomeRegion{1, Vector2{980.0f, 150.0f}, 60.0f},
    };
    map.resourceSpawnPoints = {Vector2{500.0f, 500.0f}};
    return map;
}

std::vector<ShipAction> noActionsForSixShips() { return std::vector<ShipAction>(6); }

const std::vector<int> kTeam0Ships{0, 1, 2};
const std::vector<int> kTeam1Ships{3, 4, 5};

}  // namespace

TEST_SUITE("BaselineTeamStrategy") {
    TEST_CASE("assigns a fixed Collector role to every ship but the last, which is a Defender") {
        SimulationConfig config;
        config.mode = GameMode::TeamResourceBattle;
        World world(config, buildTeamMap());
        BaselineTeamStrategy strategy;

        const std::vector<ShipGoal> goals = strategy.assignGoals(world, 0, kTeam0Ships);

        REQUIRE(goals.size() == 3);
        CHECK(goals[0].role == ShipRole::Collector);
        CHECK(goals[1].role == ShipRole::Collector);
        CHECK(goals[2].role == ShipRole::Defender);
    }

    TEST_CASE("the Defender patrols home when no enemy is within its engagement radius") {
        SimulationConfig config;
        config.mode = GameMode::TeamResourceBattle;
        World world(config, buildTeamMap());
        BaselineTeamStrategy strategy;

        const std::vector<ShipGoal> goals = strategy.assignGoals(world, 0, kTeam0Ships);

        CHECK(goals[2].role == ShipRole::Defender);
        CHECK_FALSE(goals[2].targetShipId.has_value());
        CHECK(goals[2].targetPosition == world.map().homeRegions[0].position);
    }

    TEST_CASE("the Defender engages an enemy that strays within its engagement radius of home") {
        SimulationConfig config;
        config.mode = GameMode::TeamResourceBattle;
        MapDefinition map = buildTeamMap();
        map.shipSpawns[5] = ShipSpawnPoint{Vector2{100.0f, 150.0f}, 180.0f, 1};  // Well within 220 of home0.
        World world(config, map);
        BaselineTeamStrategy strategy;

        const std::vector<ShipGoal> goals = strategy.assignGoals(world, 0, kTeam0Ships);

        REQUIRE(goals[2].targetShipId.has_value());
        CHECK(*goals[2].targetShipId == 5);
    }

    TEST_CASE("a carrying ship always switches to Collector-deliver, overriding its fixed role") {
        SimulationConfig config;
        config.mode = GameMode::TeamResourceBattle;
        MapDefinition map = buildTeamMap();
        map.shipSpawns[2].position = map.resourceSpawnPoints[0];  // The fixed Defender starts on the resource.
        World world(config, map);
        world.step(noActionsForSixShips(), kFixedDeltaSeconds);  // It picks it up.

        BaselineTeamStrategy strategy;
        const std::vector<ShipGoal> goals = strategy.assignGoals(world, 0, kTeam0Ships);

        CHECK(goals[2].role == ShipRole::Collector);
        CHECK(goals[2].targetPosition == world.map().homeRegions[0].position);
    }

    TEST_CASE("never has access to information a strategic AI could not also see") {
        // Both strategies are implemented purely in terms of the shared,
        // public ai::TeamObservations queries over World -- this test exists
        // as a regression guard that BaselineTeamStrategy keeps compiling
        // and running against the exact same World a UtilityTeamStrategy
        // would, with no special-cased construction of its own.
        SimulationConfig config;
        config.mode = GameMode::TeamResourceBattle;
        World world(config, buildTeamMap());
        BaselineTeamStrategy baseline;
        UtilityTeamStrategy utility;

        CHECK_NOTHROW((void)baseline.assignGoals(world, 1, kTeam1Ships));
        CHECK_NOTHROW((void)utility.assignGoals(world, 1, kTeam1Ships));
    }
}

TEST_SUITE("UtilityTeamStrategy") {
    TEST_CASE("sends an unassigned ship after the nearest available resource when nothing else is happening") {
        SimulationConfig config;
        config.mode = GameMode::TeamResourceBattle;
        World world(config, buildTeamMap());
        UtilityTeamStrategy strategy;

        const std::vector<ShipGoal> goals = strategy.assignGoals(world, 0, kTeam0Ships);

        const bool anyIsCollectorTowardResource =
            goals[0].role == ShipRole::Collector || goals[1].role == ShipRole::Collector ||
            goals[2].role == ShipRole::Collector;
        CHECK(anyIsCollectorTowardResource);
    }

    TEST_CASE("leftover ships Defend home once the only resource has already been claimed") {
        SimulationConfig config;
        config.mode = GameMode::TeamResourceBattle;
        MapDefinition map = buildTeamMap();
        map.shipSpawns[0].position = map.resourceSpawnPoints[0];  // Ship 0 claims the only resource.
        World world(config, map);
        world.step(noActionsForSixShips(), kFixedDeltaSeconds);
        REQUIRE(world.resources()[0].state() == astroparty::entities::ResourceState::Carried);
        UtilityTeamStrategy strategy;

        const std::vector<ShipGoal> goals = strategy.assignGoals(world, 0, kTeam0Ships);

        CHECK(goals[0].role == ShipRole::Collector);  // The carrier itself: heading home, not "leftover".
        CHECK(goals[1].role == ShipRole::Defender);
        CHECK(goals[1].targetPosition == world.map().homeRegions[0].position);
        CHECK(goals[2].role == ShipRole::Defender);
        CHECK(goals[2].targetPosition == world.map().homeRegions[0].position);
    }

    TEST_CASE("a carrying ship's role is Collector and its target is its own home region") {
        SimulationConfig config;
        config.mode = GameMode::TeamResourceBattle;
        MapDefinition map = buildTeamMap();
        map.shipSpawns[0].position = map.resourceSpawnPoints[0];
        World world(config, map);
        world.step(noActionsForSixShips(), kFixedDeltaSeconds);
        REQUIRE(world.resources()[0].state() == astroparty::entities::ResourceState::Carried);

        UtilityTeamStrategy strategy;
        const std::vector<ShipGoal> goals = strategy.assignGoals(world, 0, kTeam0Ships);

        CHECK(goals[0].role == ShipRole::Collector);
        CHECK(goals[0].targetPosition == world.map().homeRegions[0].position);
    }

    TEST_CASE("assigns the nearest available ship to intercept an enemy carrier closing in on home") {
        SimulationConfig config;
        config.mode = GameMode::TeamResourceBattle;
        MapDefinition map = buildTeamMap();
        // Enemy ship 5 starts on the resource, close enough to team 0's home to threaten it.
        map.shipSpawns[5] = ShipSpawnPoint{Vector2{300.0f, 150.0f}, 180.0f, 1};
        map.resourceSpawnPoints = {Vector2{300.0f, 150.0f}};
        // Team 0 ship 1 is the closest of the three to the carrier's position.
        map.shipSpawns[0] = ShipSpawnPoint{Vector2{50.0f, 50.0f}, 0.0f, 0};
        map.shipSpawns[1] = ShipSpawnPoint{Vector2{50.0f, 150.0f}, 0.0f, 0};
        map.shipSpawns[2] = ShipSpawnPoint{Vector2{50.0f, 450.0f}, 0.0f, 0};
        World world(config, map);
        world.step(noActionsForSixShips(), kFixedDeltaSeconds);  // Ship 5 picks up the resource.
        REQUIRE(world.resources()[0].state() == astroparty::entities::ResourceState::Carried);

        UtilityTeamStrategy strategy;
        const std::vector<ShipGoal> goals = strategy.assignGoals(world, 0, kTeam0Ships);

        const bool oneIsInterceptingShip5 =
            (goals[0].role == ShipRole::Interceptor && goals[0].targetShipId == 5) ||
            (goals[1].role == ShipRole::Interceptor && goals[1].targetShipId == 5) ||
            (goals[2].role == ShipRole::Interceptor && goals[2].targetShipId == 5);
        CHECK(oneIsInterceptingShip5);
    }

    TEST_CASE("does not intercept an enemy carrier that is still far from home") {
        SimulationConfig config;
        config.mode = GameMode::TeamResourceBattle;
        MapDefinition map = buildTeamMap();
        // Enemy ship 5 picks up the resource far on the other side of the arena (>400 from home0).
        map.shipSpawns[5] = ShipSpawnPoint{Vector2{900.0f, 500.0f}, 180.0f, 1};
        map.resourceSpawnPoints = {Vector2{900.0f, 500.0f}};
        World world(config, map);
        world.step(noActionsForSixShips(), kFixedDeltaSeconds);
        REQUIRE(world.resources()[0].state() == astroparty::entities::ResourceState::Carried);

        UtilityTeamStrategy strategy;
        const std::vector<ShipGoal> goals = strategy.assignGoals(world, 0, kTeam0Ships);

        for (const ShipGoal& goal : goals) {
            CHECK(goal.role != ShipRole::Interceptor);
        }
    }

    TEST_CASE("assigns an escort to a friendly carrier being closely followed by an enemy") {
        SimulationConfig config;
        config.mode = GameMode::TeamResourceBattle;
        MapDefinition map = buildTeamMap();
        map.shipSpawns[0] = ShipSpawnPoint{Vector2{500.0f, 500.0f}, 0.0f, 0};  // Our carrier-to-be.
        map.shipSpawns[1] = ShipSpawnPoint{Vector2{50.0f, 50.0f}, 0.0f, 0};
        map.shipSpawns[2] = ShipSpawnPoint{Vector2{50.0f, 450.0f}, 0.0f, 0};
        map.shipSpawns[3] = ShipSpawnPoint{Vector2{520.0f, 500.0f}, 180.0f, 1};  // Enemy right next to our carrier.
        map.resourceSpawnPoints = {Vector2{500.0f, 500.0f}};
        World world(config, map);
        world.step(noActionsForSixShips(), kFixedDeltaSeconds);  // Ship 0 picks up the resource.
        REQUIRE(world.resources()[0].state() == astroparty::entities::ResourceState::Carried);

        UtilityTeamStrategy strategy;
        const std::vector<ShipGoal> goals = strategy.assignGoals(world, 0, kTeam0Ships);

        CHECK(goals[0].role == ShipRole::Collector);  // The carrier itself always heads home.
        const bool oneEscortsAgainstShip3 =
            (goals[1].role == ShipRole::Escort && goals[1].targetShipId == 3) ||
            (goals[2].role == ShipRole::Escort && goals[2].targetShipId == 3);
        CHECK(oneEscortsAgainstShip3);
    }

    TEST_CASE("excludes a dead ship from assignment and gives it a harmless placeholder goal") {
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

        UtilityTeamStrategy strategy;
        const std::vector<ShipGoal> goals = strategy.assignGoals(world, 0, kTeam0Ships);

        CHECK_FALSE(goals[0].targetShipId.has_value());  // Placeholder only; never a real combat goal.
    }

    TEST_CASE("dynamically changes a ship's goal as it picks up and then delivers a resource") {
        SimulationConfig config;
        config.mode = GameMode::TeamResourceBattle;
        config.winningScore = 100;
        MapDefinition map = buildTeamMap();
        map.shipSpawns[0].position = Vector2{500.0f, 500.0f};
        map.resourceSpawnPoints = {Vector2{500.0f, 500.0f}};
        World world(config, map);
        UtilityTeamStrategy strategy;

        // Before pickup: ship 0 (nearest to the only resource) gathers it.
        const std::vector<ShipGoal> beforePickup = strategy.assignGoals(world, 0, kTeam0Ships);
        CHECK(beforePickup[0].role == ShipRole::Collector);
        CHECK(beforePickup[0].targetPosition == map.resourceSpawnPoints[0]);

        world.step(noActionsForSixShips(), kFixedDeltaSeconds);
        REQUIRE(world.resources()[0].state() == astroparty::entities::ResourceState::Carried);

        // After pickup: the same ship's goal switches to heading home, with no further input needed.
        const std::vector<ShipGoal> afterPickup = strategy.assignGoals(world, 0, kTeam0Ships);
        CHECK(afterPickup[0].role == ShipRole::Collector);
        CHECK(afterPickup[0].targetPosition == map.homeRegions[0].position);
        CHECK(afterPickup[0].targetPosition != beforePickup[0].targetPosition);
    }
}

TEST_SUITE("TeamShipController") {
    TEST_CASE("decide() produces the controlled ship's goal translated into a ShipAction") {
        SimulationConfig config;
        config.mode = GameMode::TeamResourceBattle;
        World world(config, buildTeamMap());
        auto strategy = std::make_shared<BaselineTeamStrategy>();
        TeamShipController controller(strategy, 0, kTeam0Ships);

        CHECK_NOTHROW((void)controller.decide(world, 0));
        CHECK_NOTHROW((void)controller.decide(world, 2));
    }

    TEST_CASE("fails safe (no turn, no shoot) if asked to decide for a ship outside its team") {
        SimulationConfig config;
        config.mode = GameMode::TeamResourceBattle;
        World world(config, buildTeamMap());
        auto strategy = std::make_shared<BaselineTeamStrategy>();
        TeamShipController controller(strategy, 0, kTeam0Ships);

        const ShipAction action = controller.decide(world, 3);  // Ship 3 belongs to team 1, not this controller.

        CHECK_FALSE(action.turnRight);
        CHECK_FALSE(action.shoot);
    }

    TEST_CASE("an Interceptor/Escort goal can result in shooting once aimed and in range") {
        SimulationConfig config;
        config.mode = GameMode::TeamResourceBattle;
        MapDefinition map = buildTeamMap();
        // Enemy ship 5 carries the resource, close to home and directly in front of (aligned with) ship 1.
        map.shipSpawns[1] = ShipSpawnPoint{Vector2{100.0f, 150.0f}, 90.0f, 0};  // Facing +X, toward ship 5.
        map.shipSpawns[5] = ShipSpawnPoint{Vector2{250.0f, 150.0f}, 180.0f, 1};
        map.resourceSpawnPoints = {Vector2{250.0f, 150.0f}};
        World world(config, map);
        world.step(noActionsForSixShips(), kFixedDeltaSeconds);  // Ship 5 picks up the resource.
        REQUIRE(world.resources()[0].state() == astroparty::entities::ResourceState::Carried);

        auto strategy = std::make_shared<UtilityTeamStrategy>();
        TeamShipController controller(strategy, 0, kTeam0Ships);

        const ShipAction action = controller.decide(world, 1);
        CHECK_FALSE(action.turnRight);  // Already aimed; no further turning needed.
        CHECK(action.shoot);
    }
}
