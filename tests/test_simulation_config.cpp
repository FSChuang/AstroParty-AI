#include <doctest/doctest.h>

#include <filesystem>
#include <fstream>

#include "astroparty/core/SimulationConfig.hpp"

using astroparty::core::SimulationConfig;

namespace {

// Writes `contents` to a uniquely named temporary file and returns its path.
// Used to exercise SimulationConfig::loadFromFile without depending on any
// checked-in fixture file.
std::filesystem::path writeTempFile(const std::string& fileName, const std::string& contents) {
    const std::filesystem::path path = std::filesystem::temp_directory_path() / fileName;
    std::ofstream file(path);
    file << contents;
    return path;
}

}  // namespace

TEST_SUITE("SimulationConfig") {
    TEST_CASE("the default configuration is valid") { CHECK_NOTHROW(SimulationConfig{}.validate()); }

    TEST_CASE("validate rejects a non-positive arena dimension") {
        SimulationConfig config;
        config.arenaSize.x = 0.0f;
        CHECK_THROWS_AS(config.validate(), std::invalid_argument);
    }

    TEST_CASE("validate rejects a non-positive tick rate") {
        SimulationConfig config;
        config.tickRateHz = -60.0f;
        CHECK_THROWS_AS(config.validate(), std::invalid_argument);
    }

    TEST_CASE("validate rejects a non-positive ship max speed") {
        SimulationConfig config;
        config.shipMaxSpeedUnitsPerSecond = 0.0f;
        CHECK_THROWS_AS(config.validate(), std::invalid_argument);
    }

    TEST_CASE("validate rejects a negative maxMatchTicks") {
        SimulationConfig config;
        config.maxMatchTicks = -1;
        CHECK_THROWS_AS(config.validate(), std::invalid_argument);
    }

    TEST_CASE("validate rejects a ship collision radius too large for the arena") {
        SimulationConfig config;
        config.arenaSize = {20.0f, 20.0f};
        config.shipCollisionRadius = 18.0f;  // 2x radius (36) exceeds the 20-unit arena.
        CHECK_THROWS_AS(config.validate(), std::invalid_argument);
    }

    TEST_CASE("fixedDeltaSeconds is the reciprocal of the tick rate") {
        SimulationConfig config;
        config.tickRateHz = 50.0f;
        CHECK(config.fixedDeltaSeconds() == doctest::Approx(0.02f));
    }

    TEST_CASE("loadFromFile applies only the fields present, keeping other defaults") {
        const auto path = writeTempFile("astroparty_config_partial.json", R"({"tickRateHz": 120.0})");

        const SimulationConfig config = SimulationConfig::loadFromFile(path);

        CHECK(config.tickRateHz == doctest::Approx(120.0f));
        CHECK(config.shipMaxSpeedUnitsPerSecond == doctest::Approx(SimulationConfig{}.shipMaxSpeedUnitsPerSecond));
    }

    TEST_CASE("loadFromFile applies every recognized field") {
        const auto path = writeTempFile("astroparty_config_full.json", R"({
            "arenaWidth": 1000.0,
            "arenaHeight": 500.0,
            "tickRateHz": 30.0,
            "shipAccelerationUnitsPerSecondSquared": 10.0,
            "shipMaxSpeedUnitsPerSecond": 100.0,
            "shipTurnRateDegreesPerSecond": 200.0,
            "shipCollisionRadius": 15.0,
            "bulletSpeedUnitsPerSecond": 300.0,
            "bulletLifetimeSeconds": 2.0,
            "bulletSpawnOffset": 20.0,
            "maxMatchTicks": 1800
        })");

        const SimulationConfig config = SimulationConfig::loadFromFile(path);

        CHECK(config.arenaSize.x == doctest::Approx(1000.0f));
        CHECK(config.arenaSize.y == doctest::Approx(500.0f));
        CHECK(config.tickRateHz == doctest::Approx(30.0f));
        CHECK(config.shipAccelerationUnitsPerSecondSquared == doctest::Approx(10.0f));
        CHECK(config.shipMaxSpeedUnitsPerSecond == doctest::Approx(100.0f));
        CHECK(config.shipTurnRateDegreesPerSecond == doctest::Approx(200.0f));
        CHECK(config.shipCollisionRadius == doctest::Approx(15.0f));
        CHECK(config.bulletSpeedUnitsPerSecond == doctest::Approx(300.0f));
        CHECK(config.bulletLifetimeSeconds == doctest::Approx(2.0f));
        CHECK(config.bulletSpawnOffset == doctest::Approx(20.0f));
        CHECK(config.maxMatchTicks == 1800);
    }

    TEST_CASE("loadFromFile throws for a file that does not exist") {
        CHECK_THROWS_AS((void)SimulationConfig::loadFromFile("this/path/does/not/exist.json"), std::runtime_error);
    }

    TEST_CASE("loadFromFile throws for malformed JSON") {
        const auto path = writeTempFile("astroparty_config_malformed.json", "{ not valid json");
        CHECK_THROWS_AS((void)SimulationConfig::loadFromFile(path), std::runtime_error);
    }

    TEST_CASE("loadFromFile throws when parsed values fail validation") {
        const auto path = writeTempFile("astroparty_config_invalid.json", R"({"tickRateHz": -5.0})");
        CHECK_THROWS_AS((void)SimulationConfig::loadFromFile(path), std::invalid_argument);
    }
}
