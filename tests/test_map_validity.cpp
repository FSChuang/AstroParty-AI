#include <doctest/doctest.h>

#include <vector>

#include "astroparty/core/MapDefinition.hpp"
#include "astroparty/procgen/MapValidity.hpp"

using astroparty::core::HomeRegion;
using astroparty::core::MapDefinition;
using astroparty::core::ObstacleDefinition;
using astroparty::core::ShipSpawnPoint;
using astroparty::core::Vector2;
using astroparty::procgen::checkGeometricReachability;

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

// A full-height wall of touching obstacles at x == wallX, sealing the
// arena into a left half and a right half.
std::vector<ObstacleDefinition> fullHeightWall(float wallX, float arenaHeight, float obstacleRadius) {
    std::vector<ObstacleDefinition> wall;
    for (float y = 0.0f; y <= arenaHeight; y += obstacleRadius * 1.5f) {
        wall.push_back(ObstacleDefinition{{wallX, y}, obstacleRadius});
    }
    return wall;
}

}  // namespace

TEST_SUITE("procgen::checkGeometricReachability") {
    TEST_CASE("an obstacle-free team map is fully connected") {
        const MapDefinition map = buildOpenTeamMap();

        const auto report = checkGeometricReachability(map, kShipClearanceRadius, kCellSize);

        CHECK(report.team0SpawnsCanReachOwnHome);
        CHECK(report.team1SpawnsCanReachOwnHome);
        CHECK(report.team0HomeCanReachEveryResource);
        CHECK(report.team1HomeCanReachEveryResource);
        CHECK(report.isFullyConnected());
    }

    TEST_CASE("a wall sealing the resource away from team 0's home breaks only that requirement") {
        MapDefinition map = buildOpenTeamMap();
        // The resource sits at x=400; a wall just to its left, spanning
        // from team 0's side up to (but not past) the resource, blocks
        // only team 0's approach -- team 1 (on the right) is unaffected.
        map.obstacles = fullHeightWall(380.0f, map.arenaSize.y, 25.0f);

        const auto report = checkGeometricReachability(map, kShipClearanceRadius, kCellSize);

        CHECK_FALSE(report.team0HomeCanReachEveryResource);
        CHECK(report.team1HomeCanReachEveryResource);
        CHECK_FALSE(report.isFullyConnected());
    }

    TEST_CASE("a wall between a team's spawns and its own home breaks that requirement") {
        MapDefinition map = buildOpenTeamMap();
        // Team 0's spawns are at x=50; its home is at x=20. A wall at
        // x=35 separates them.
        map.obstacles = fullHeightWall(35.0f, map.arenaSize.y, 20.0f);

        const auto report = checkGeometricReachability(map, kShipClearanceRadius, kCellSize);

        CHECK_FALSE(report.team0SpawnsCanReachOwnHome);
    }

    TEST_CASE("a map with no resource spawn points is reported as not reaching every resource") {
        MapDefinition map = buildOpenTeamMap();
        map.resourceSpawnPoints.clear();

        const auto report = checkGeometricReachability(map, kShipClearanceRadius, kCellSize);

        CHECK_FALSE(report.team0HomeCanReachEveryResource);
        CHECK_FALSE(report.team1HomeCanReachEveryResource);
    }
}
