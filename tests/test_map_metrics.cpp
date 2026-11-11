#include <doctest/doctest.h>

#include "astroparty/core/MapDefinition.hpp"
#include "astroparty/procgen/MapMetrics.hpp"

using astroparty::core::HomeRegion;
using astroparty::core::MapDefinition;
using astroparty::core::ObstacleDefinition;
using astroparty::core::ShipSpawnPoint;
using astroparty::core::Vector2;
using astroparty::procgen::computeMapMetrics;

namespace {

constexpr float kShipClearanceRadius = 18.0f;
constexpr float kCellSize = 10.0f;
constexpr float kClusterGapThreshold = 20.0f;

MapDefinition buildSymmetricTeamMap() {
    MapDefinition map;
    map.arenaSize = Vector2{1000.0f, 600.0f};
    map.shipSpawns = {
        ShipSpawnPoint{{60.0f, 150.0f}, 0.0f, 0}, ShipSpawnPoint{{60.0f, 300.0f}, 0.0f, 0},
        ShipSpawnPoint{{60.0f, 450.0f}, 0.0f, 0}, ShipSpawnPoint{{940.0f, 150.0f}, 0.0f, 1},
        ShipSpawnPoint{{940.0f, 300.0f}, 0.0f, 1}, ShipSpawnPoint{{940.0f, 450.0f}, 0.0f, 1},
    };
    map.homeRegions = {HomeRegion{0, {30.0f, 300.0f}, 40.0f}, HomeRegion{1, {970.0f, 300.0f}, 40.0f}};
    map.resourceSpawnPoints = {{500.0f, 300.0f}};  // Exactly on the midline: perfectly symmetric by construction.
    return map;
}

}  // namespace

TEST_SUITE("procgen::computeMapMetrics") {
    TEST_CASE("obstacleCoverageRatio matches the exact analytic sum-of-circle-areas formula") {
        MapDefinition map = buildSymmetricTeamMap();
        map.obstacles = {ObstacleDefinition{{300.0f, 300.0f}, 50.0f}, ObstacleDefinition{{700.0f, 300.0f}, 50.0f}};

        const auto metrics = computeMapMetrics(map, kShipClearanceRadius, kCellSize, kClusterGapThreshold);

        const float expectedArea = 2.0f * 3.14159265f * 50.0f * 50.0f;
        const float expectedRatio = expectedArea / (1000.0f * 600.0f);
        CHECK(metrics.obstacleCoverageRatio == doctest::Approx(expectedRatio).epsilon(0.001));
    }

    TEST_CASE("obstacleCoverageRatio is zero for an obstacle-free map") {
        const MapDefinition map = buildSymmetricTeamMap();

        const auto metrics = computeMapMetrics(map, kShipClearanceRadius, kCellSize, kClusterGapThreshold);

        CHECK(metrics.obstacleCoverageRatio == doctest::Approx(0.0f));
    }

    TEST_CASE("a perfectly symmetric map has near-zero team accessibility imbalance") {
        const MapDefinition map = buildSymmetricTeamMap();

        const auto metrics = computeMapMetrics(map, kShipClearanceRadius, kCellSize, kClusterGapThreshold);

        CHECK(metrics.team0NavigationPathLength == doctest::Approx(metrics.team1NavigationPathLength).epsilon(0.02));
        CHECK(metrics.teamAccessibilityImbalance < 10.0f);  // Grid-discretization noise only, not a real asymmetry.
    }

    TEST_CASE("an asymmetric obstacle layout produces a measurable team accessibility imbalance") {
        MapDefinition map = buildSymmetricTeamMap();
        // A large obstacle directly between team 0's home and the resource,
        // with nothing comparable on team 1's side.
        map.obstacles = {ObstacleDefinition{{250.0f, 300.0f}, 60.0f}};

        const auto metrics = computeMapMetrics(map, kShipClearanceRadius, kCellSize, kClusterGapThreshold);

        CHECK(metrics.team0NavigationPathLength > metrics.team1NavigationPathLength);
        CHECK(metrics.teamAccessibilityImbalance > 20.0f);
    }

    TEST_CASE("obstacleClusterCount counts two separated groups as two clusters") {
        MapDefinition map = buildSymmetricTeamMap();
        // Two obstacles close together (one cluster) plus one far away (a second cluster).
        map.obstacles = {ObstacleDefinition{{300.0f, 280.0f}, 30.0f}, ObstacleDefinition{{300.0f, 330.0f}, 30.0f},
                          ObstacleDefinition{{700.0f, 300.0f}, 30.0f}};

        const auto metrics = computeMapMetrics(map, kShipClearanceRadius, kCellSize, kClusterGapThreshold);

        CHECK(metrics.obstacleClusterCount == 2);
    }

    TEST_CASE("obstacleClusterCount counts every obstacle separately when none are close enough to group") {
        MapDefinition map = buildSymmetricTeamMap();
        map.obstacles = {ObstacleDefinition{{200.0f, 300.0f}, 20.0f}, ObstacleDefinition{{500.0f, 100.0f}, 20.0f},
                          ObstacleDefinition{{800.0f, 500.0f}, 20.0f}};

        const auto metrics = computeMapMetrics(map, kShipClearanceRadius, kCellSize, kClusterGapThreshold);

        CHECK(metrics.obstacleClusterCount == 3);
    }

    TEST_CASE("reachableFreeSpaceFraction is 1.0 for a fully open, obstacle-free map") {
        const MapDefinition map = buildSymmetricTeamMap();

        const auto metrics = computeMapMetrics(map, kShipClearanceRadius, kCellSize, kClusterGapThreshold);

        CHECK(metrics.reachableFreeSpaceFraction == doctest::Approx(1.0f));
    }
}
