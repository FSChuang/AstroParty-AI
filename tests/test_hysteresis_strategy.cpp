#include <doctest/doctest.h>

#include <optional>
#include <vector>

#include "astroparty/ai/HysteresisTeamStrategy.hpp"
#include "astroparty/core/GameMode.hpp"
#include "astroparty/core/MapDefinition.hpp"
#include "astroparty/core/SimulationConfig.hpp"
#include "astroparty/core/Vector2.hpp"
#include "astroparty/core/World.hpp"
#include "astroparty/entities/Resource.hpp"

using astroparty::ai::HysteresisTeamStrategy;
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
    map.name = "hysteresis_test_map";
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

TEST_SUITE("HysteresisTeamStrategy") {
    TEST_CASE("assigns an Interceptor as soon as an enemy carrier enters the same entry radius Utility uses") {
        SimulationConfig config;
        config.mode = GameMode::TeamResourceBattle;
        MapDefinition map = buildTeamMap();
        map.shipSpawns[5] = ShipSpawnPoint{Vector2{300.0f, 150.0f}, 180.0f, 1};  // Within 400 of home0.
        map.resourceSpawnPoints = {Vector2{300.0f, 150.0f}};
        World world(config, map);
        world.step(noActionsForSixShips(), kFixedDeltaSeconds);  // Ship 5 picks it up.
        REQUIRE(world.resources()[0].state() == astroparty::entities::ResourceState::Carried);

        HysteresisTeamStrategy strategy;
        const std::vector<ShipGoal> goals = strategy.assignGoals(world, 0, kTeam0Ships);

        const bool oneIsIntercepting =
            (goals[0].role == ShipRole::Interceptor && goals[0].targetShipId == 5) ||
            (goals[1].role == ShipRole::Interceptor && goals[1].targetShipId == 5) ||
            (goals[2].role == ShipRole::Interceptor && goals[2].targetShipId == 5);
        CHECK(oneIsIntercepting);
    }

    TEST_CASE("repeated calls within the same tick return identical goals (shared-strategy call collapsing)") {
        SimulationConfig config;
        config.mode = GameMode::TeamResourceBattle;
        MapDefinition map = buildTeamMap();
        map.shipSpawns[5] = ShipSpawnPoint{Vector2{300.0f, 150.0f}, 180.0f, 1};
        map.resourceSpawnPoints = {Vector2{300.0f, 150.0f}};
        World world(config, map);
        world.step(noActionsForSixShips(), kFixedDeltaSeconds);

        HysteresisTeamStrategy strategy;
        const std::vector<ShipGoal> first = strategy.assignGoals(world, 0, kTeam0Ships);
        const std::vector<ShipGoal> second = strategy.assignGoals(world, 0, kTeam0Ships);
        const std::vector<ShipGoal> third = strategy.assignGoals(world, 0, kTeam0Ships);

        for (std::size_t i = 0; i < first.size(); ++i) {
            CHECK(first[i].role == second[i].role);
            CHECK(first[i].role == third[i].role);
            CHECK(first[i].targetShipId == second[i].targetShipId);
            CHECK(first[i].targetPosition == second[i].targetPosition);
        }
    }

    TEST_CASE("an Interceptor commitment stays on the same ship and target across consecutive ticks") {
        SimulationConfig config;
        config.mode = GameMode::TeamResourceBattle;
        MapDefinition map = buildTeamMap();
        map.shipSpawns[5] = ShipSpawnPoint{Vector2{300.0f, 150.0f}, 180.0f, 1};
        map.resourceSpawnPoints = {Vector2{300.0f, 150.0f}};
        World world(config, map);
        world.step(noActionsForSixShips(), kFixedDeltaSeconds);

        HysteresisTeamStrategy strategy;
        const std::vector<ShipGoal> firstTickGoals = strategy.assignGoals(world, 0, kTeam0Ships);
        std::optional<std::size_t> interceptorIndex;
        for (std::size_t i = 0; i < firstTickGoals.size(); ++i) {
            if (firstTickGoals[i].role == ShipRole::Interceptor) interceptorIndex = i;
        }
        REQUIRE(interceptorIndex.has_value());

        for (int i = 0; i < 10; ++i) {
            world.step(noActionsForSixShips(), kFixedDeltaSeconds);
            const std::vector<ShipGoal> goals = strategy.assignGoals(world, 0, kTeam0Ships);
            CHECK(goals[*interceptorIndex].role == ShipRole::Interceptor);
            CHECK(goals[*interceptorIndex].targetShipId == 5);
        }
    }

    TEST_CASE("a carrying ship always overrides any existing commitment and heads home immediately") {
        SimulationConfig config;
        config.mode = GameMode::TeamResourceBattle;
        MapDefinition map = buildTeamMap();
        map.shipSpawns[0].position = map.resourceSpawnPoints[0];
        World world(config, map);
        world.step(noActionsForSixShips(), kFixedDeltaSeconds);  // Ship 0 picks it up.
        REQUIRE(world.resources()[0].state() == astroparty::entities::ResourceState::Carried);

        HysteresisTeamStrategy strategy;
        const std::vector<ShipGoal> goals = strategy.assignGoals(world, 0, kTeam0Ships);

        CHECK(goals[0].role == ShipRole::Collector);
        CHECK(goals[0].targetPosition == world.map().homeRegions[0].position);
    }

    TEST_CASE("an eliminated commitment target causes immediate reassignment") {
        SimulationConfig config;
        config.mode = GameMode::TeamResourceBattle;
        MapDefinition map = buildTeamMap();
        map.shipSpawns[5] = ShipSpawnPoint{Vector2{300.0f, 150.0f}, 180.0f, 1};
        // Facing ship 5, close enough to shoot it quickly, but outside the
        // resource's pickup radius so ship 0 does not grab it instead of ship 5.
        map.shipSpawns[0] = ShipSpawnPoint{Vector2{380.0f, 150.0f}, 270.0f, 0};
        map.resourceSpawnPoints = {Vector2{300.0f, 150.0f}};
        World world(config, map);
        world.step(noActionsForSixShips(), kFixedDeltaSeconds);  // Ship 5 picks up the resource.
        REQUIRE(world.resources()[0].state() == astroparty::entities::ResourceState::Carried);

        HysteresisTeamStrategy strategy;
        const std::vector<ShipGoal> beforeDeath = strategy.assignGoals(world, 0, kTeam0Ships);
        const bool wasIntercepting = beforeDeath[0].role == ShipRole::Interceptor && beforeDeath[0].targetShipId == 5;
        REQUIRE(wasIntercepting);

        std::vector<ShipAction> shipZeroFires = noActionsForSixShips();
        shipZeroFires[0].shoot = true;
        world.step(shipZeroFires, kFixedDeltaSeconds);
        bool shipFiveDied = false;
        for (int i = 0; i < 150 && !shipFiveDied; ++i) {
            world.step(noActionsForSixShips(), kFixedDeltaSeconds);
            shipFiveDied = !world.ships()[5].isAlive();
        }
        REQUIRE(shipFiveDied);

        const std::vector<ShipGoal> afterDeath = strategy.assignGoals(world, 0, kTeam0Ships);
        CHECK_FALSE((afterDeath[0].role == ShipRole::Interceptor && afterDeath[0].targetShipId == 5));
    }
}
