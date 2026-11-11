#include <doctest/doctest.h>

#include <filesystem>
#include <fstream>

#include "astroparty/core/MapDefinition.hpp"

using astroparty::core::HomeRegion;
using astroparty::core::MapDefinition;
using astroparty::core::ObstacleDefinition;
using astroparty::core::ShipSpawnPoint;
using astroparty::core::Vector2;

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

    TEST_CASE("validate rejects a spawn point placed inside an obstacle") {
        MapDefinition map = MapDefinition::defaultTwoShipMap();
        // The first spawn is at (120, 120); place an obstacle large enough to engulf it.
        map.obstacles.push_back({{120.0f, 120.0f}, 50.0f});
        CHECK_THROWS_AS(map.validate(), std::invalid_argument);
    }

    TEST_CASE("validate accepts a spawn point that merely sits near, not inside, an obstacle") {
        MapDefinition map = MapDefinition::defaultTwoShipMap();
        map.obstacles.push_back({{120.0f, 120.0f}, 5.0f});  // spawn is exactly at the obstacle's center scaled away
        map.shipSpawns[0].position = Vector2{200.0f, 120.0f};  // 80 units from the obstacle, well clear of radius 5
        CHECK_NOTHROW(map.validate());
    }

    TEST_CASE("validate rejects a map that assigns a team to only some ship spawns") {
        MapDefinition map = MapDefinition::defaultTwoShipMap();
        map.shipSpawns[0].teamId = 0;
        // shipSpawns[1].teamId is left at -1 -- an inconsistent mix.
        CHECK_THROWS_AS(map.validate(), std::invalid_argument);
    }

    TEST_CASE("validate accepts a map where every spawn has a team assigned") {
        MapDefinition map = MapDefinition::defaultTwoShipMap();
        map.shipSpawns[0].teamId = 0;
        map.shipSpawns[1].teamId = 1;
        CHECK_NOTHROW(map.validate());
    }

    TEST_CASE("validate rejects a home region with a non-positive radius") {
        MapDefinition map = MapDefinition::defaultTwoShipMap();
        map.shipSpawns[0].teamId = 0;
        map.shipSpawns[1].teamId = 1;
        map.homeRegions.push_back(HomeRegion{0, Vector2{50.0f, 50.0f}, 0.0f});
        CHECK_THROWS_AS(map.validate(), std::invalid_argument);
    }

    TEST_CASE("validate rejects a home region with a negative teamId") {
        MapDefinition map = MapDefinition::defaultTwoShipMap();
        map.homeRegions.push_back(HomeRegion{-1, Vector2{50.0f, 50.0f}, 30.0f});
        CHECK_THROWS_AS(map.validate(), std::invalid_argument);
    }

    TEST_CASE("validate rejects a resource spawn point outside the arena") {
        MapDefinition map = MapDefinition::defaultTwoShipMap();
        map.resourceSpawnPoints.push_back(Vector2{map.arenaSize.x + 50.0f, 0.0f});
        CHECK_THROWS_AS(map.validate(), std::invalid_argument);
    }

    TEST_CASE("validate accepts a well-formed Team Resource Battle map") {
        MapDefinition map;
        map.name = "team_test_map";
        map.arenaSize = Vector2{1000.0f, 1000.0f};
        map.shipSpawns = {
            ShipSpawnPoint{Vector2{50.0f, 50.0f}, 0.0f, 0},
            ShipSpawnPoint{Vector2{50.0f, 150.0f}, 0.0f, 0},
            ShipSpawnPoint{Vector2{50.0f, 250.0f}, 0.0f, 0},
            ShipSpawnPoint{Vector2{950.0f, 50.0f}, 180.0f, 1},
            ShipSpawnPoint{Vector2{950.0f, 150.0f}, 180.0f, 1},
            ShipSpawnPoint{Vector2{950.0f, 250.0f}, 180.0f, 1},
        };
        map.homeRegions = {
            HomeRegion{0, Vector2{20.0f, 150.0f}, 60.0f},
            HomeRegion{1, Vector2{980.0f, 150.0f}, 60.0f},
        };
        map.resourceSpawnPoints = {Vector2{500.0f, 500.0f}};

        CHECK_NOTHROW(map.validate());
    }

    TEST_CASE("loadFromFile parses teamId, homeRegions, and resourceSpawnPoints") {
        const auto path = writeTempFile("astroparty_map_team.json", R"({
            "name": "team_json_map",
            "arenaSize": {"x": 1000.0, "y": 1000.0},
            "shipSpawns": [
                {"position": {"x": 50.0, "y": 50.0}, "teamId": 0},
                {"position": {"x": 50.0, "y": 150.0}, "teamId": 0},
                {"position": {"x": 50.0, "y": 250.0}, "teamId": 0},
                {"position": {"x": 950.0, "y": 50.0}, "teamId": 1},
                {"position": {"x": 950.0, "y": 150.0}, "teamId": 1},
                {"position": {"x": 950.0, "y": 250.0}, "teamId": 1}
            ],
            "homeRegions": [
                {"teamId": 0, "position": {"x": 20.0, "y": 150.0}, "radius": 60.0},
                {"teamId": 1, "position": {"x": 980.0, "y": 150.0}, "radius": 60.0}
            ],
            "resourceSpawnPoints": [
                {"x": 500.0, "y": 500.0}
            ]
        })");

        const MapDefinition map = MapDefinition::loadFromFile(path);

        CHECK(map.shipSpawns[0].teamId == 0);
        CHECK(map.shipSpawns[3].teamId == 1);
        REQUIRE(map.homeRegions.size() == 2);
        CHECK(map.homeRegions[0].teamId == 0);
        CHECK(map.homeRegions[0].radius == doctest::Approx(60.0f));
        REQUIRE(map.resourceSpawnPoints.size() == 1);
        CHECK(map.resourceSpawnPoints[0].x == doctest::Approx(500.0f));
        CHECK_NOTHROW(map.validate());
    }

    TEST_CASE("loadFromFile defaults teamId to -1 when omitted") {
        const auto path = writeTempFile("astroparty_map_no_team.json", R"({
            "arenaSize": {"x": 400.0, "y": 300.0},
            "shipSpawns": [
                {"position": {"x": 20.0, "y": 20.0}},
                {"position": {"x": 380.0, "y": 280.0}}
            ]
        })");

        const MapDefinition map = MapDefinition::loadFromFile(path);

        CHECK(map.shipSpawns[0].teamId == -1);
        CHECK(map.shipSpawns[1].teamId == -1);
    }
}

TEST_SUITE("MapDefinition serialization") {
    TEST_CASE("toJsonString then loadFromFile round-trips a team map exactly") {
        MapDefinition original;
        original.name = "roundtrip_test";
        original.arenaSize = Vector2{1000.0f, 800.0f};
        original.shipSpawns = {
            ShipSpawnPoint{Vector2{50.0f, 50.0f}, 10.0f, 0},
            ShipSpawnPoint{Vector2{50.0f, 150.0f}, 20.0f, 0},
            ShipSpawnPoint{Vector2{50.0f, 250.0f}, 30.0f, 0},
            ShipSpawnPoint{Vector2{950.0f, 50.0f}, 190.0f, 1},
            ShipSpawnPoint{Vector2{950.0f, 150.0f}, 200.0f, 1},
            ShipSpawnPoint{Vector2{950.0f, 250.0f}, 210.0f, 1},
        };
        original.obstacles = {ObstacleDefinition{Vector2{400.0f, 400.0f}, 35.0f},
                               ObstacleDefinition{Vector2{600.0f, 400.0f}, 42.0f}};
        original.homeRegions = {HomeRegion{0, Vector2{20.0f, 150.0f}, 60.0f}, HomeRegion{1, Vector2{980.0f, 150.0f}, 60.0f}};
        original.resourceSpawnPoints = {Vector2{500.0f, 400.0f}, Vector2{500.0f, 200.0f}};

        const auto path = std::filesystem::temp_directory_path() / "astroparty_roundtrip_test_map.json";
        original.saveToFile(path);
        const MapDefinition reloaded = MapDefinition::loadFromFile(path);

        CHECK(reloaded.name == original.name);
        CHECK(reloaded.arenaSize == original.arenaSize);
        REQUIRE(reloaded.shipSpawns.size() == original.shipSpawns.size());
        for (std::size_t i = 0; i < original.shipSpawns.size(); ++i) {
            CHECK(reloaded.shipSpawns[i].position == original.shipSpawns[i].position);
            CHECK(reloaded.shipSpawns[i].headingDegrees == doctest::Approx(original.shipSpawns[i].headingDegrees));
            CHECK(reloaded.shipSpawns[i].teamId == original.shipSpawns[i].teamId);
        }
        REQUIRE(reloaded.obstacles.size() == original.obstacles.size());
        for (std::size_t i = 0; i < original.obstacles.size(); ++i) {
            CHECK(reloaded.obstacles[i].position == original.obstacles[i].position);
            CHECK(reloaded.obstacles[i].radius == doctest::Approx(original.obstacles[i].radius));
        }
        REQUIRE(reloaded.homeRegions.size() == original.homeRegions.size());
        for (std::size_t i = 0; i < original.homeRegions.size(); ++i) {
            CHECK(reloaded.homeRegions[i].teamId == original.homeRegions[i].teamId);
            CHECK(reloaded.homeRegions[i].position == original.homeRegions[i].position);
            CHECK(reloaded.homeRegions[i].radius == doctest::Approx(original.homeRegions[i].radius));
        }
        CHECK(reloaded.resourceSpawnPoints == original.resourceSpawnPoints);
        CHECK_NOTHROW(reloaded.validate());
    }

    TEST_CASE("a round-tripped map still satisfies Team Resource Battle structure") {
        MapDefinition map;
        map.arenaSize = Vector2{1000.0f, 800.0f};
        map.shipSpawns = {
            ShipSpawnPoint{{50.0f, 50.0f}, 0.0f, 0},   ShipSpawnPoint{{50.0f, 150.0f}, 0.0f, 0},
            ShipSpawnPoint{{50.0f, 250.0f}, 0.0f, 0},  ShipSpawnPoint{{950.0f, 50.0f}, 0.0f, 1},
            ShipSpawnPoint{{950.0f, 150.0f}, 0.0f, 1}, ShipSpawnPoint{{950.0f, 250.0f}, 0.0f, 1},
        };
        map.homeRegions = {HomeRegion{0, {20.0f, 150.0f}, 60.0f}, HomeRegion{1, {980.0f, 150.0f}, 60.0f}};
        map.resourceSpawnPoints = {{500.0f, 400.0f}};

        CHECK_NOTHROW(astroparty::core::validateTeamResourceBattleStructure(map));
    }
}

TEST_SUITE("validateTeamResourceBattleStructure") {
    TEST_CASE("throws when a team does not have exactly three ships") {
        MapDefinition map;
        map.arenaSize = Vector2{1000.0f, 800.0f};
        map.shipSpawns = {ShipSpawnPoint{{50.0f, 50.0f}, 0.0f, 0}, ShipSpawnPoint{{950.0f, 50.0f}, 0.0f, 1}};
        map.homeRegions = {HomeRegion{0, {20.0f, 150.0f}, 60.0f}, HomeRegion{1, {980.0f, 150.0f}, 60.0f}};
        map.resourceSpawnPoints = {{500.0f, 400.0f}};

        CHECK_THROWS_AS(astroparty::core::validateTeamResourceBattleStructure(map), std::invalid_argument);
    }

    TEST_CASE("throws when a home region is missing for a team") {
        MapDefinition map;
        map.arenaSize = Vector2{1000.0f, 800.0f};
        map.shipSpawns = {
            ShipSpawnPoint{{50.0f, 50.0f}, 0.0f, 0},   ShipSpawnPoint{{50.0f, 150.0f}, 0.0f, 0},
            ShipSpawnPoint{{50.0f, 250.0f}, 0.0f, 0},  ShipSpawnPoint{{950.0f, 50.0f}, 0.0f, 1},
            ShipSpawnPoint{{950.0f, 150.0f}, 0.0f, 1}, ShipSpawnPoint{{950.0f, 250.0f}, 0.0f, 1},
        };
        map.homeRegions = {HomeRegion{0, {20.0f, 150.0f}, 60.0f}};
        map.resourceSpawnPoints = {{500.0f, 400.0f}};

        CHECK_THROWS_AS(astroparty::core::validateTeamResourceBattleStructure(map), std::invalid_argument);
    }

    TEST_CASE("throws when there are no resource spawn points") {
        MapDefinition map;
        map.arenaSize = Vector2{1000.0f, 800.0f};
        map.shipSpawns = {
            ShipSpawnPoint{{50.0f, 50.0f}, 0.0f, 0},   ShipSpawnPoint{{50.0f, 150.0f}, 0.0f, 0},
            ShipSpawnPoint{{50.0f, 250.0f}, 0.0f, 0},  ShipSpawnPoint{{950.0f, 50.0f}, 0.0f, 1},
            ShipSpawnPoint{{950.0f, 150.0f}, 0.0f, 1}, ShipSpawnPoint{{950.0f, 250.0f}, 0.0f, 1},
        };
        map.homeRegions = {HomeRegion{0, {20.0f, 150.0f}, 60.0f}, HomeRegion{1, {980.0f, 150.0f}, 60.0f}};

        CHECK_THROWS_AS(astroparty::core::validateTeamResourceBattleStructure(map), std::invalid_argument);
    }
}
