#include <doctest/doctest.h>

#include <filesystem>
#include <fstream>

#include "astroparty/procgen/MapGeneratorConfig.hpp"

using astroparty::procgen::MapGeneratorConfig;

namespace {
std::filesystem::path writeTempFile(const std::string& fileName, const std::string& contents) {
    const std::filesystem::path path = std::filesystem::temp_directory_path() / fileName;
    std::ofstream file(path);
    file << contents;
    return path;
}
}  // namespace

TEST_SUITE("procgen::MapGeneratorConfig") {
    TEST_CASE("default-constructed config validates") { CHECK_NOTHROW(MapGeneratorConfig{}.validate()); }

    TEST_CASE("validate rejects a non-positive arena dimension") {
        MapGeneratorConfig config;
        config.arenaSize.x = 0.0f;
        CHECK_THROWS_AS(config.validate(), std::invalid_argument);
    }

    TEST_CASE("validate rejects obstacleRadiusMin greater than obstacleRadiusMax") {
        MapGeneratorConfig config;
        config.obstacleRadiusMin = 100.0f;
        config.obstacleRadiusMax = 50.0f;
        CHECK_THROWS_AS(config.validate(), std::invalid_argument);
    }

    TEST_CASE("validate rejects a negative obstacleCount") {
        MapGeneratorConfig config;
        config.obstacleCount = -1;
        CHECK_THROWS_AS(config.validate(), std::invalid_argument);
    }

    TEST_CASE("validate accepts obstacleCount of zero (an obstacle-free arena)") {
        MapGeneratorConfig config;
        config.obstacleCount = 0;
        CHECK_NOTHROW(config.validate());
    }

    TEST_CASE("validate rejects a negative clusterCount") {
        MapGeneratorConfig config;
        config.clusterCount = -2;
        CHECK_THROWS_AS(config.validate(), std::invalid_argument);
    }

    TEST_CASE("validate rejects a clusterCenterBandWidthFraction of zero or above one") {
        MapGeneratorConfig config;
        config.clusterCenterBandWidthFraction = 0.0f;
        CHECK_THROWS_AS(config.validate(), std::invalid_argument);

        config.clusterCenterBandWidthFraction = 1.5f;
        CHECK_THROWS_AS(config.validate(), std::invalid_argument);
    }

    TEST_CASE("validate rejects a baseMarginFraction outside (0, 0.5)") {
        MapGeneratorConfig config;
        config.baseMarginFraction = 0.6f;
        CHECK_THROWS_AS(config.validate(), std::invalid_argument);
    }

    TEST_CASE("validate rejects a non-positive maxPlacementAttemptsPerObject") {
        MapGeneratorConfig config;
        config.maxPlacementAttemptsPerObject = 0;
        CHECK_THROWS_AS(config.validate(), std::invalid_argument);
    }

    TEST_CASE("loadFromFile parses every field and keeps defaults for omitted ones") {
        const auto path = writeTempFile("astroparty_mapgen_config_test.json", R"({
            "obstacleCount": 14,
            "clusterCount": 3,
            "symmetric": false
        })");

        const MapGeneratorConfig config = MapGeneratorConfig::loadFromFile(path);

        CHECK(config.obstacleCount == 14);
        CHECK(config.clusterCount == 3);
        CHECK_FALSE(config.symmetric);
        CHECK(config.resourceCount == MapGeneratorConfig{}.resourceCount);  // Untouched, keeps the default.
    }

    TEST_CASE("loadFromFile throws when the parsed config fails validation") {
        const auto path = writeTempFile("astroparty_mapgen_config_invalid_test.json", R"({
            "obstacleCount": -5
        })");

        CHECK_THROWS_AS((void)MapGeneratorConfig::loadFromFile(path), std::invalid_argument);
    }

    TEST_CASE("loadFromFile throws for a missing file") {
        CHECK_THROWS_AS((void)MapGeneratorConfig::loadFromFile("/nonexistent/path/config.json"), std::runtime_error);
    }
}
