#include <doctest/doctest.h>

#include <filesystem>
#include <fstream>

#include "astroparty/core/MapDefinition.hpp"

using astroparty::core::MapDefinition;
using astroparty::core::ShipSpawnPoint;

namespace {

std::filesystem::path writeTempFile(const std::string& fileName, const std::string& contents) {
    const std::filesystem::path path = std::filesystem::temp_directory_path() / fileName;
    std::ofstream file(path);
    file << contents;
    return path;
}

}  // namespace

TEST_SUITE("MapDefinition") {
    TEST_CASE("the built-in default two-ship map is valid") { CHECK_NOTHROW(MapDefinition::defaultTwoShipMap().validate()); }

    TEST_CASE("validate rejects a map with fewer than two spawn points") {
        MapDefinition map = MapDefinition::defaultTwoShipMap();
        map.shipSpawns.resize(1);
        CHECK_THROWS_AS(map.validate(), std::invalid_argument);
    }

    TEST_CASE("validate rejects a spawn point outside the arena bounds") {
        MapDefinition map = MapDefinition::defaultTwoShipMap();
        map.shipSpawns.push_back(ShipSpawnPoint{{map.arenaSize.x + 100.0f, 0.0f}, 0.0f});
        CHECK_THROWS_AS(map.validate(), std::invalid_argument);
    }

    TEST_CASE("validate rejects an obstacle with a non-positive radius") {
        MapDefinition map = MapDefinition::defaultTwoShipMap();
        map.obstacles.push_back({{100.0f, 100.0f}, 0.0f});
        CHECK_THROWS_AS(map.validate(), std::invalid_argument);
    }

    TEST_CASE("validate rejects a non-positive arena dimension") {
        MapDefinition map = MapDefinition::defaultTwoShipMap();
        map.arenaSize.y = -1.0f;
        CHECK_THROWS_AS(map.validate(), std::invalid_argument);
    }

    TEST_CASE("loadFromFile parses spawns, obstacles, and the map name") {
        const auto path = writeTempFile("astroparty_map_full.json", R"({
            "name": "test_arena",
            "arenaSize": {"x": 800.0, "y": 600.0},
            "shipSpawns": [
                {"position": {"x": 50.0, "y": 50.0}, "headingDegrees": 45.0},
                {"position": {"x": 750.0, "y": 550.0}, "headingDegrees": 225.0}
            ],
            "obstacles": [
                {"position": {"x": 400.0, "y": 300.0}, "radius": 30.0}
            ]
        })");

        const MapDefinition map = MapDefinition::loadFromFile(path);

        CHECK(map.name == "test_arena");
        CHECK(map.arenaSize.x == doctest::Approx(800.0f));
        CHECK(map.arenaSize.y == doctest::Approx(600.0f));
        REQUIRE(map.shipSpawns.size() == 2);
        CHECK(map.shipSpawns[0].position.x == doctest::Approx(50.0f));
        CHECK(map.shipSpawns[0].headingDegrees == doctest::Approx(45.0f));
        REQUIRE(map.obstacles.size() == 1);
        CHECK(map.obstacles[0].radius == doctest::Approx(30.0f));
        CHECK_NOTHROW(map.validate());
    }

    TEST_CASE("loadFromFile defaults headingDegrees to zero and obstacles to empty when omitted") {
        const auto path = writeTempFile("astroparty_map_minimal.json", R"({
            "arenaSize": {"x": 400.0, "y": 300.0},
            "shipSpawns": [
                {"position": {"x": 20.0, "y": 20.0}},
                {"position": {"x": 380.0, "y": 280.0}}
            ]
        })");

        const MapDefinition map = MapDefinition::loadFromFile(path);

        CHECK(map.shipSpawns[0].headingDegrees == doctest::Approx(0.0f));
        CHECK(map.obstacles.empty());
    }

    TEST_CASE("loadFromFile throws for a file that does not exist") {
        CHECK_THROWS_AS((void)MapDefinition::loadFromFile("this/path/does/not/exist.json"), std::runtime_error);
    }

    TEST_CASE("loadFromFile throws for malformed JSON") {
        const auto path = writeTempFile("astroparty_map_malformed.json", "{ not valid json");
        CHECK_THROWS_AS((void)MapDefinition::loadFromFile(path), std::runtime_error);
    }

    TEST_CASE("loadFromFile throws when a required field is missing") {
        const auto path = writeTempFile("astroparty_map_missing_field.json", R"({"arenaSize": {"x": 100.0, "y": 100.0}})");
        CHECK_THROWS_AS((void)MapDefinition::loadFromFile(path), std::runtime_error);
    }

    TEST_CASE("loadFromFile throws when the parsed map fails structural validation") {
        const auto path = writeTempFile("astroparty_map_invalid.json", R"({
            "arenaSize": {"x": 400.0, "y": 300.0},
            "shipSpawns": [
                {"position": {"x": 20.0, "y": 20.0}}
            ]
        })");
        CHECK_THROWS_AS((void)MapDefinition::loadFromFile(path), std::invalid_argument);
    }
}
