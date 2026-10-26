// Guards against the example config/map files checked into configs/ going
// stale: if the JSON schema these loaders accept ever changes incompatibly,
// these tests fail here instead of a teammate discovering it by hand.
#include <doctest/doctest.h>

#include <filesystem>

#include "astroparty/core/MapDefinition.hpp"
#include "astroparty/core/SimulationConfig.hpp"

using astroparty::core::MapDefinition;
using astroparty::core::SimulationConfig;

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
}
