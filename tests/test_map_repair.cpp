#include <doctest/doctest.h>

#include <vector>

#include "astroparty/core/MapDefinition.hpp"
#include "astroparty/procgen/MapRepair.hpp"
#include "astroparty/procgen/MapValidity.hpp"

using astroparty::core::HomeRegion;
using astroparty::core::MapDefinition;
using astroparty::core::ObstacleDefinition;
using astroparty::core::ShipSpawnPoint;
using astroparty::core::Vector2;
using astroparty::procgen::checkGeometricReachability;
using astroparty::procgen::repairForReachability;

namespace {

constexpr float kShipClearanceRadius = 18.0f;
constexpr float kCellSize = 15.0f;

MapDefinition buildOpenTeamMap() {
    MapDefinition map;
    map.arenaSize = Vector2{800.0f, 600.0f};
    map.shipSpawns = {
        ShipSpawnPoint{{50.0f, 100.0f}, 0.0f, 0},  ShipSpawnPoint{{50.0f, 300.0f}, 0.0f, 0},
        ShipSpawnPoint{{50.0f, 500.0f}, 0.0f, 0},  ShipSpawnPoint{{750.0f, 100.0f}, 0.0f, 1},
        ShipSpawnPoint{{750.0f, 300.0f}, 0.0f, 1}, ShipSpawnPoint{{750.0f, 500.0f}, 0.0f, 1},
    };
    map.homeRegions = {HomeRegion{0, {20.0f, 300.0f}, 40.0f}, HomeRegion{1, {780.0f, 300.0f}, 40.0f}};
    map.resourceSpawnPoints = {{400.0f, 300.0f}};
    return map;
}

std::vector<ObstacleDefinition> fullHeightWall(float wallX, float arenaHeight, float obstacleRadius) {
    std::vector<ObstacleDefinition> wall;
    for (float y = 0.0f; y <= arenaHeight; y += obstacleRadius * 1.5f) {
        wall.push_back(ObstacleDefinition{{wallX, y}, obstacleRadius});
    }
    return wall;
}

}  // namespace

TEST_SUITE("procgen::repairForReachability") {
    TEST_CASE("restores reachability by removing obstacles from a sealing wall") {
        MapDefinition map = buildOpenTeamMap();
        map.obstacles = fullHeightWall(380.0f, map.arenaSize.y, 25.0f);
        REQUIRE_FALSE(checkGeometricReachability(map, kShipClearanceRadius, kCellSize).isFullyConnected());

        const auto result = repairForReachability(map, kShipClearanceRadius, kCellSize, /*maxRemovals=*/10);

        CHECK(result.reachabilityRestored);
        CHECK(result.obstaclesRemoved > 0);
        CHECK(result.obstaclesRemoved <= 10);
        CHECK(checkGeometricReachability(result.map, kShipClearanceRadius, kCellSize).isFullyConnected());
    }

    TEST_CASE("never removes more than maxRemovals obstacles") {
        MapDefinition map = buildOpenTeamMap();
        map.obstacles = fullHeightWall(380.0f, map.arenaSize.y, 25.0f);

        const auto result = repairForReachability(map, kShipClearanceRadius, kCellSize, /*maxRemovals=*/1);

        CHECK(result.obstaclesRemoved <= 1);
    }

    TEST_CASE("reports failure (not a false success) when the removal budget is too small to fix it") {
        MapDefinition map = buildOpenTeamMap();
        // A much longer, denser wall needs more than one removal to open a gap.
        map.obstacles = fullHeightWall(380.0f, map.arenaSize.y, 25.0f);

        const auto result = repairForReachability(map, kShipClearanceRadius, kCellSize, /*maxRemovals=*/1);

        CHECK_FALSE(result.reachabilityRestored);
    }

    TEST_CASE("an already-connected map is returned unmodified") {
        const MapDefinition map = buildOpenTeamMap();

        const auto result = repairForReachability(map, kShipClearanceRadius, kCellSize, /*maxRemovals=*/5);

        CHECK(result.reachabilityRestored);
        CHECK(result.obstaclesRemoved == 0);
        CHECK(result.map.obstacles.size() == map.obstacles.size());
    }

    TEST_CASE("never modifies ship spawns, home regions, or resource spawn points") {
        MapDefinition map = buildOpenTeamMap();
        map.obstacles = fullHeightWall(380.0f, map.arenaSize.y, 25.0f);

        const auto result = repairForReachability(map, kShipClearanceRadius, kCellSize, /*maxRemovals=*/10);

        REQUIRE(result.map.shipSpawns.size() == map.shipSpawns.size());
        for (std::size_t i = 0; i < map.shipSpawns.size(); ++i) {
            CHECK(result.map.shipSpawns[i].position == map.shipSpawns[i].position);
            CHECK(result.map.shipSpawns[i].teamId == map.shipSpawns[i].teamId);
        }
        REQUIRE(result.map.homeRegions.size() == map.homeRegions.size());
        for (std::size_t i = 0; i < map.homeRegions.size(); ++i) {
            CHECK(result.map.homeRegions[i].position == map.homeRegions[i].position);
        }
        CHECK(result.map.resourceSpawnPoints == map.resourceSpawnPoints);
    }

    TEST_CASE("a maxRemovals of zero reports failure immediately without touching the map") {
        MapDefinition map = buildOpenTeamMap();
        map.obstacles = fullHeightWall(380.0f, map.arenaSize.y, 25.0f);

        const auto result = repairForReachability(map, kShipClearanceRadius, kCellSize, /*maxRemovals=*/0);

        CHECK_FALSE(result.reachabilityRestored);
        CHECK(result.obstaclesRemoved == 0);
        CHECK(result.map.obstacles.size() == map.obstacles.size());
    }
}
