#include <doctest/doctest.h>

#include <memory>
#include <vector>

#include <nlohmann/json.hpp>

#include "astroparty/ai/BaselineTeamStrategy.hpp"
#include "astroparty/ai/TeamShipController.hpp"
#include "astroparty/ai/TeamStrategy.hpp"
#include "astroparty/core/GameMode.hpp"
#include "astroparty/core/MapDefinition.hpp"
#include "astroparty/core/SimulationConfig.hpp"
#include "astroparty/core/Vector2.hpp"
#include "astroparty/core/World.hpp"
#include "astroparty/headless/TeamMatchRunner.hpp"
#include "astroparty/input/ShipController.hpp"

using astroparty::ai::BaselineTeamStrategy;
using astroparty::core::GameMode;
using astroparty::core::HomeRegion;
using astroparty::core::MapDefinition;
using astroparty::core::ShipSpawnPoint;
using astroparty::core::SimulationConfig;
using astroparty::core::Vector2;
using astroparty::core::World;
using astroparty::headless::runTeamMatch;
using astroparty::headless::TeamMatchResult;
using astroparty::headless::TeamMatchResultOutcome;
using astroparty::input::IShipController;

namespace {

MapDefinition buildTeamMap() {
    MapDefinition map;
    map.name = "team_runner_test_map";
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

// One BaselineTeamStrategy-driven controller per ship, grouped by team --
// the same wiring headless_main.cpp performs, factored out so every test
// here builds its controllers identically.
std::vector<std::unique_ptr<IShipController>> makeBaselineControllers() {
    auto team0Strategy = std::make_shared<BaselineTeamStrategy>();
    auto team1Strategy = std::make_shared<BaselineTeamStrategy>();
    std::vector<std::unique_ptr<IShipController>> controllers;
    controllers.push_back(std::make_unique<astroparty::ai::TeamShipController>(team0Strategy, 0,
                                                                                 std::vector<int>{0, 1, 2}));
    controllers.push_back(std::make_unique<astroparty::ai::TeamShipController>(team0Strategy, 0,
                                                                                 std::vector<int>{0, 1, 2}));
    controllers.push_back(std::make_unique<astroparty::ai::TeamShipController>(team0Strategy, 0,
                                                                                 std::vector<int>{0, 1, 2}));
    controllers.push_back(std::make_unique<astroparty::ai::TeamShipController>(team1Strategy, 1,
                                                                                 std::vector<int>{3, 4, 5}));
    controllers.push_back(std::make_unique<astroparty::ai::TeamShipController>(team1Strategy, 1,
                                                                                 std::vector<int>{3, 4, 5}));
    controllers.push_back(std::make_unique<astroparty::ai::TeamShipController>(team1Strategy, 1,
                                                                                 std::vector<int>{3, 4, 5}));
    return controllers;
}

}  // namespace

TEST_SUITE("headless::runTeamMatch") {
    TEST_CASE("a team that reaches the winning score wins the match") {
        SimulationConfig config;
        config.mode = GameMode::TeamResourceBattle;
        config.winningScore = 1;
        MapDefinition map = buildTeamMap();
        const Vector2 spot{20.0f, 150.0f};  // Inside team 0's home region.
        map.shipSpawns[0].position = spot;
        map.resourceSpawnPoints = {spot};
        World world(config, map);
        auto controllers = makeBaselineControllers();

        const TeamMatchResult result = runTeamMatch(world, controllers, /*maxTicks=*/10);

        CHECK(result.outcome == TeamMatchResultOutcome::TeamWon);
        REQUIRE(result.winningTeamId.has_value());
        CHECK(*result.winningTeamId == 0);
        CHECK(result.teamScores[0] == 1);
    }

    TEST_CASE("a match that never resolves times out at the runner's own maxTicks") {
        SimulationConfig config;
        config.mode = GameMode::TeamResourceBattle;
        config.winningScore = 100;    // Unreachable within the short run below.
        config.maxMatchTicks = 0;     // No tick-budget resolution inside World itself either.
        MapDefinition map = buildTeamMap();
        World world(config, map);
        auto controllers = makeBaselineControllers();

        const TeamMatchResult result = runTeamMatch(world, controllers, /*maxTicks=*/5);

        CHECK(result.outcome == TeamMatchResultOutcome::Timeout);
        CHECK_FALSE(result.winningTeamId.has_value());
        CHECK(result.ticksElapsed == 5);
    }

    TEST_CASE("runTeamMatch rejects a world not in TeamResourceBattle mode") {
        World world;  // Default Classic Deathmatch world.
        std::vector<std::unique_ptr<IShipController>> controllers;
        controllers.push_back(std::make_unique<astroparty::ai::TeamShipController>(
            std::make_shared<BaselineTeamStrategy>(), 0, std::vector<int>{0}));
        controllers.push_back(std::make_unique<astroparty::ai::TeamShipController>(
            std::make_shared<BaselineTeamStrategy>(), 1, std::vector<int>{1}));

        CHECK_THROWS_AS((void)runTeamMatch(world, controllers, 10), std::invalid_argument);
    }

    TEST_CASE("runTeamMatch rejects a controller count that does not match the ship count") {
        SimulationConfig config;
        config.mode = GameMode::TeamResourceBattle;
        World world(config, buildTeamMap());
        auto controllers = makeBaselineControllers();
        controllers.pop_back();  // Now five controllers for six ships.

        CHECK_THROWS_AS((void)runTeamMatch(world, controllers, 10), std::invalid_argument);
    }

    TEST_CASE("runTeamMatch rejects a non-positive maxTicks") {
        SimulationConfig config;
        config.mode = GameMode::TeamResourceBattle;
        World world(config, buildTeamMap());
        auto controllers = makeBaselineControllers();

        CHECK_THROWS_AS((void)runTeamMatch(world, controllers, 0), std::invalid_argument);
    }

    TEST_CASE("identical configuration, map, and controllers produce identical results") {
        auto runOnce = []() {
            SimulationConfig config;
            config.mode = GameMode::TeamResourceBattle;
            World world(config, buildTeamMap());
            auto controllers = makeBaselineControllers();
            return runTeamMatch(world, controllers, 200);
        };

        const TeamMatchResult first = runOnce();
        const TeamMatchResult second = runOnce();

        CHECK(first.outcome == second.outcome);
        CHECK(first.winningTeamId == second.winningTeamId);
        CHECK(first.ticksElapsed == second.ticksElapsed);
        CHECK(first.teamScores == second.teamScores);
        CHECK(first.resourcesCollected == second.resourcesCollected);
        CHECK(first.shipsDestroyed == second.shipsDestroyed);
    }

    TEST_CASE("a deterministic run also reaches an identical final World state") {
        auto runOnceAndCaptureFinalState = []() {
            SimulationConfig config;
            config.mode = GameMode::TeamResourceBattle;
            World world(config, buildTeamMap());
            auto controllers = makeBaselineControllers();
            (void)runTeamMatch(world, controllers, 200);
            return std::make_pair(world.ships()[0].position(), world.ships()[5].velocity());
        };

        const auto [positionA, velocityA] = runOnceAndCaptureFinalState();
        const auto [positionB, velocityB] = runOnceAndCaptureFinalState();

        CHECK(positionA == positionB);
        CHECK(velocityA == velocityB);
    }

    TEST_CASE("TeamMatchResult serializes to the documented JSON shape") {
        TeamMatchResult result;
        result.outcome = TeamMatchResultOutcome::TeamWon;
        result.winningTeamId = 0;
        result.ticksElapsed = 500;
        result.teamScores = {3, 1};
        result.resourcesCollected = {4, 2};
        result.shipsDestroyed = {1, 3};

        const nlohmann::json json = nlohmann::json::parse(result.toJsonString());

        CHECK(json.at("outcome").get<std::string>() == "TeamWon");
        CHECK(json.at("winningTeamId").get<int>() == 0);
        CHECK(json.at("ticksElapsed").get<int>() == 500);
        CHECK(json.at("teamScores").get<std::vector<int>>() == std::vector<int>{3, 1});
        CHECK(json.at("resourcesCollected").get<std::vector<int>>() == std::vector<int>{4, 2});
        CHECK(json.at("shipsDestroyed").get<std::vector<int>>() == std::vector<int>{1, 3});
    }

    TEST_CASE("TeamMatchResult serializes a null winningTeamId for a draw or timeout") {
        TeamMatchResult result;
        result.outcome = TeamMatchResultOutcome::Draw;
        result.winningTeamId = std::nullopt;

        const nlohmann::json json = nlohmann::json::parse(result.toJsonString());

        CHECK(json.at("outcome").get<std::string>() == "Draw");
        CHECK(json.at("winningTeamId").is_null());
    }
}
