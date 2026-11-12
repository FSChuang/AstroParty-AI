#include "astroparty/procgen/MapMetrics.hpp"

#include <cmath>
#include <functional>
#include <numeric>
#include <optional>
#include <vector>

#include "astroparty/procgen/OccupancyGrid.hpp"

namespace astroparty::procgen {

using core::HomeRegion;
using core::MapDefinition;
using core::ObstacleDefinition;
using core::Vector2;

namespace {

constexpr float kPi = 3.14159265358979323846f;

float distanceBetween(Vector2 a, Vector2 b) noexcept { return std::sqrt(core::distanceSquared(a, b)); }

float computeObstacleCoverageRatio(const MapDefinition& map) {
    float obstacleArea = 0.0f;
    for (const ObstacleDefinition& obstacle : map.obstacles) {
        obstacleArea += kPi * obstacle.radius * obstacle.radius;
    }
    const float arenaArea = map.arenaSize.x * map.arenaSize.y;
    return arenaArea > 0.0f ? obstacleArea / arenaArea : 0.0f;
}

std::optional<Vector2> homePositionForTeam(const MapDefinition& map, int teamId) {
    for (const HomeRegion& home : map.homeRegions) {
        if (home.teamId == teamId) return home.position;
    }
    return std::nullopt;
}

float averagePathLengthFromHome(Vector2 home, const MapDefinition& map, const OccupancyGrid& grid) {
    float total = 0.0f;
    int reachableCount = 0;
    for (const Vector2& resource : map.resourceSpawnPoints) {
        if (const auto length = grid.shortestPathLength(home, resource)) {
            total += *length;
            ++reachableCount;
        }
    }
    return reachableCount > 0 ? total / static_cast<float>(reachableCount) : 0.0f;
}

// Union-find over obstacle indices: two obstacles are joined if the gap
// between their edges is within `clusterGapThreshold`. The number of
// distinct roots after processing every pair is the spatial cluster count.
int countSpatialClusters(const std::vector<ObstacleDefinition>& obstacles, float clusterGapThreshold) {
    if (obstacles.empty()) {
        return 0;
    }
    std::vector<std::size_t> parent(obstacles.size());
    std::iota(parent.begin(), parent.end(), 0);

    std::function<std::size_t(std::size_t)> find = [&](std::size_t i) -> std::size_t {
        while (parent[i] != i) {
            parent[i] = parent[parent[i]];
            i = parent[i];
        }
        return i;
    };

    for (std::size_t i = 0; i < obstacles.size(); ++i) {
        for (std::size_t j = i + 1; j < obstacles.size(); ++j) {
            const float gap =
                distanceBetween(obstacles[i].position, obstacles[j].position) - obstacles[i].radius - obstacles[j].radius;
            if (gap <= clusterGapThreshold) {
                const std::size_t rootI = find(i);
                const std::size_t rootJ = find(j);
                if (rootI != rootJ) {
                    parent[rootI] = rootJ;
                }
            }
        }
    }

    int clusterCount = 0;
    for (std::size_t i = 0; i < obstacles.size(); ++i) {
        if (find(i) == i) {
            ++clusterCount;
        }
    }
    return clusterCount;
}

}  // namespace

MapMetrics computeMapMetrics(const MapDefinition& map, float shipClearanceRadius, float cellSize,
                              float clusterGapThreshold) {
    MapMetrics metrics;
    metrics.obstacleCoverageRatio = computeObstacleCoverageRatio(map);
    metrics.obstacleClusterCount = countSpatialClusters(map.obstacles, clusterGapThreshold);

    const OccupancyGrid grid(map.arenaSize, map.obstacles, shipClearanceRadius, cellSize);

    if (const auto home0 = homePositionForTeam(map, 0)) {
        metrics.team0NavigationPathLength = averagePathLengthFromHome(*home0, map, grid);
        metrics.reachableFreeSpaceFraction = grid.reachableFreeSpaceFraction(*home0);
    }
    if (const auto home1 = homePositionForTeam(map, 1)) {
        metrics.team1NavigationPathLength = averagePathLengthFromHome(*home1, map, grid);
    }
    metrics.teamAccessibilityImbalance = std::abs(metrics.team0NavigationPathLength - metrics.team1NavigationPathLength);

    return metrics;
}

}  // namespace astroparty::procgen
