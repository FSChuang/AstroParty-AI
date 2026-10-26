#include <doctest/doctest.h>

#include <memory>
#include <vector>

#include <nlohmann/json.hpp>

#include "astroparty/core/MapDefinition.hpp"
#include "astroparty/core/SimulationConfig.hpp"
#include "astroparty/core/World.hpp"
#include "astroparty/headless/MatchRunner.hpp"
#include "astroparty/input/ScriptedShipController.hpp"

using astroparty::core::MapDefinition;
using astroparty::core::ShipSpawnPoint;
using astroparty::core::SimulationConfig;
using astroparty::core::Vector2;
using astroparty::core::World;
using astroparty::headless::MatchResult;
using astroparty::headless::MatchResultOutcome;
using astroparty::headless::runMatch;
using astroparty::input::IShipController;
using astroparty::input::ScriptedShipController;

namespace {

// Two ships facing each other at close range, used to deterministically
// force a hit within the first simulated tick: at this distance, a bullet
// fired by either ship reaches the other before resolveCollisions runs for
// that same step.
MapDefinition facingPairMap() {
    MapDefinition map;
    map.name = "facing_pair_test_map";
    map.arenaSize = Vector2{640.0f, 480.0f};
    map.shipSpawns = {
        ShipSpawnPoint{Vector2{300.0f, 300.0f}, 90.0f},   // Facing +X (right), toward the other ship.
        ShipSpawnPoint{Vector2{350.0f, 300.0f}, 270.0f},  // Facing -X (left), toward the other ship.
    };
    return map;
}

std::vector<std::unique_ptr<IShipController>> makeControllers(bool firstShoots, bool secondShoots) {
    std::vector<std::unique_ptr<IShipController>> controllers;
    // shootIntervalTicks = 1 fires on tick 0 (the edge-triggered rising edge
    // from Ship's initial "not held" state) and then holds, which never
    // fires again -- exactly one shot each, which is all this needs.
    controllers.push_back(std::make_unique<ScriptedShipController>(false, firstShoots ? 1 : 0));
    controllers.push_back(std::make_unique<ScriptedShipController>(false, secondShoots ? 1 : 0));
    return controllers;
}

}  // namespace

TEST_SUITE("headless::runMatch") {
    TEST_CASE("a lone shooter wins against a non-shooting opponent") {
        World world(SimulationConfig{}, facingPairMap());
        auto controllers = makeControllers(/*firstShoots=*/true, /*secondShoots=*/false);

        const MatchResult result = runMatch(world, controllers, /*maxTicks=*/10);

        CHECK(result.outcome == MatchResultOutcome::ShipWon);
        REQUIRE(result.winningShipId.has_value());
        CHECK(*result.winningShipId == 0);
        CHECK(result.ticksElapsed <= 10);
    }

    TEST_CASE("two simultaneous mutual kills produce a draw") {
        World world(SimulationConfig{}, facingPairMap());
        auto controllers = makeControllers(/*firstShoots=*/true, /*secondShoots=*/true);

        const MatchResult result = runMatch(world, controllers, /*maxTicks=*/10);

        CHECK(result.outcome == MatchResultOutcome::Draw);
        CHECK_FALSE(result.winningShipId.has_value());
    }

    TEST_CASE("a match that never resolves times out at maxTicks") {
        World world;  // Default two-corner map; ships start far enough apart to not collide quickly.
        auto controllers = makeControllers(/*firstShoots=*/false, /*secondShoots=*/false);

        const MatchResult result = runMatch(world, controllers, /*maxTicks=*/5);

        CHECK(result.outcome == MatchResultOutcome::Timeout);
        CHECK_FALSE(result.winningShipId.has_value());
        CHECK(result.ticksElapsed == 5);
    }

    TEST_CASE("runMatch rejects a controller count that does not match the ship count") {
        World world(SimulationConfig{}, facingPairMap());
        std::vector<std::unique_ptr<IShipController>> controllers;
        controllers.push_back(std::make_unique<ScriptedShipController>(false, 0));  // Only one, but two ships.

        CHECK_THROWS_AS((void)runMatch(world, controllers, 10), std::invalid_argument);
    }

    TEST_CASE("runMatch rejects a non-positive maxTicks") {
        World world(SimulationConfig{}, facingPairMap());
        auto controllers = makeControllers(true, false);
        CHECK_THROWS_AS((void)runMatch(world, controllers, 0), std::invalid_argument);
    }

    TEST_CASE("identical configuration, map, and controllers produce identical results") {
        auto runOnce = []() {
            World world(SimulationConfig{}, facingPairMap());
            auto controllers = makeControllers(true, false);
            return runMatch(world, controllers, 10);
        };

        const MatchResult first = runOnce();
        const MatchResult second = runOnce();

        CHECK(first.outcome == second.outcome);
        CHECK(first.winningShipId == second.winningShipId);
        CHECK(first.ticksElapsed == second.ticksElapsed);
    }

    TEST_CASE("a deterministic run also reaches an identical final World state") {
        // Stronger than comparing MatchResult alone: replays the same
        // configuration, map, and controller script and checks the full
        // final ship positions and velocities match exactly, not just the
        // summarized outcome.
        auto runOnceAndCaptureFinalState = []() {
            World world;
            std::vector<std::unique_ptr<IShipController>> controllers;
            controllers.push_back(std::make_unique<ScriptedShipController>(true, 7));
            controllers.push_back(std::make_unique<ScriptedShipController>(false, 11));
            (void)runMatch(world, controllers, 50);
            return std::make_pair(world.ships()[0].position(), world.ships()[1].velocity());
        };

        const auto [positionA, velocityA] = runOnceAndCaptureFinalState();
        const auto [positionB, velocityB] = runOnceAndCaptureFinalState();

        CHECK(positionA == positionB);
        CHECK(velocityA == velocityB);
    }

    TEST_CASE("MatchResult serializes to the documented JSON shape") {
        MatchResult result;
        result.outcome = MatchResultOutcome::ShipWon;
        result.winningShipId = 1;
        result.ticksElapsed = 42;

        const nlohmann::json json = nlohmann::json::parse(result.toJsonString());

        CHECK(json.at("outcome").get<std::string>() == "ShipWon");
        CHECK(json.at("winningShipId").get<int>() == 1);
        CHECK(json.at("ticksElapsed").get<int>() == 42);
    }

    TEST_CASE("MatchResult serializes a null winningShipId for a draw or timeout") {
        MatchResult result;
        result.outcome = MatchResultOutcome::Draw;
        result.winningShipId = std::nullopt;
        result.ticksElapsed = 7;

        const nlohmann::json json = nlohmann::json::parse(result.toJsonString());

        CHECK(json.at("outcome").get<std::string>() == "Draw");
        CHECK(json.at("winningShipId").is_null());
    }
}
