// Guards against the example config/map files checked into configs/ going
// stale: if the JSON schema these loaders accept ever changes incompatibly,
// these tests fail here instead of a teammate discovering it by hand.
#include <doctest/doctest.h>

#include <filesystem>

#include "astroparty/core/GameMode.hpp"
#include "astroparty/core/MapDefinition.hpp"
#include "astroparty/core/SimulationConfig.hpp"
#include "astroparty/core/World.hpp"

using astroparty::core::GameMode;
using astroparty::core::MapDefinition;
using astroparty::core::SimulationConfig;
using astroparty::core::World;

namespace {
const std::filesystem::path kConfigsDir = ASTROPARTY_CONFIGS_DIR;
}

TEST_SUITE("example configuration and map assets") {
    TEST_CASE("configs/default_simulation.json loads and validates") {
        const SimulationConfig config = SimulationConfig::loadFromFile(kConfigsDir / "default_simulation.json");
        CHECK_NOTHROW(config.validate());
        // This file documents Milestone 1's exact original values; confirm
        // it still matches the struct's own compiled-in defaults.
        CHECK(config.arenaSize.x == doctest::Approx(SimulationConfig{}.arenaSize.x));
        CHECK(config.shipMaxSpeedUnitsPerSecond == doctest::Approx(SimulationConfig{}.shipMaxSpeedUnitsPerSecond));
    }

    TEST_CASE("configs/maps/default_arena.json loads and validates") {
        const MapDefinition map = MapDefinition::loadFromFile(kConfigsDir / "maps/default_arena.json");
        CHECK_NOTHROW(map.validate());
        CHECK(map.shipSpawns.size() == 2);
        CHECK(map.obstacles.empty());
    }

    TEST_CASE("configs/maps/example_with_obstacles.json loads and validates") {
        const MapDefinition map = MapDefinition::loadFromFile(kConfigsDir / "maps/example_with_obstacles.json");
        CHECK_NOTHROW(map.validate());
        CHECK(map.shipSpawns.size() == 2);
        CHECK(map.obstacles.size() == 2);
    }

    TEST_CASE("configs/maps/team_arena.json loads, validates, and constructs a Team Resource Battle World") {
        const MapDefinition map = MapDefinition::loadFromFile(kConfigsDir / "maps/team_arena.json");
        CHECK_NOTHROW(map.validate());
        CHECK(map.shipSpawns.size() == 6);
        CHECK(map.obstacles.size() == 4);
        CHECK(map.homeRegions.size() == 2);
        CHECK(map.resourceSpawnPoints.size() == 5);

        SimulationConfig config;
        config.mode = GameMode::TeamResourceBattle;
        // World's constructor itself enforces the full 3v3 + home region +
        // resource structural requirements for this mode -- constructing
        // successfully here is the real end-to-end check that this fixed
        // arena is actually playable, not just individually well-formed.
        CHECK_NOTHROW(World(config, map));
    }

    TEST_CASE("configs/team_resource_battle.json loads, validates, and pairs with team_arena.json") {
        const SimulationConfig config = SimulationConfig::loadFromFile(kConfigsDir / "team_resource_battle.json");
        CHECK_NOTHROW(config.validate());
        CHECK(config.mode == GameMode::TeamResourceBattle);

        const MapDefinition map = MapDefinition::loadFromFile(kConfigsDir / "maps/team_arena.json");
        CHECK_NOTHROW(World(config, map));
    }
}
