#include "astroparty/procgen/MapRepair.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <optional>
#include <utility>

#include "astroparty/procgen/OccupancyGrid.hpp"

namespace astroparty::procgen {

using core::HomeRegion;
using core::MapDefinition;
using core::ShipSpawnPoint;
using core::Vector2;

namespace {

using Segment = std::pair<Vector2, Vector2>;

std::optional<Vector2> homePositionForTeam(const MapDefinition& map, int teamId) {
    for (const HomeRegion& home : map.homeRegions) {
        if (home.teamId == teamId) return home.position;
    }
    return std::nullopt;
}

// Returns the two points that still need connecting, or std::nullopt if
// every requirement is already satisfied. Checked in a fixed priority
// order (spawns-to-home before home-to-resources) -- when several
// requirements fail at once, repair always targets whichever this order
// considers first, then rechecks everything from scratch on the next
// iteration.
std::optional<Segment> findFirstUnsatisfiedSegment(const MapDefinition& map, const OccupancyGrid& grid) {
    for (const int teamId : {0, 1}) {
        const std::optional<Vector2> home = homePositionForTeam(map, teamId);
        if (!home) continue;
        for (const ShipSpawnPoint& spawn : map.shipSpawns) {
            if (spawn.teamId == teamId && !grid.isReachable(spawn.position, *home)) {
                return Segment{spawn.position, *home};
            }
        }
    }
    for (const int teamId : {0, 1}) {
        const std::optional<Vector2> home = homePositionForTeam(map, teamId);
        if (!home) continue;
        for (const Vector2& resource : map.resourceSpawnPoints) {
            if (!grid.isReachable(*home, resource)) {
                return Segment{*home, resource};
            }
        }
    }
    return std::nullopt;
}

float distanceBetween(Vector2 a, Vector2 b) noexcept { return std::sqrt(core::distanceSquared(a, b)); }

float pointToSegmentDistance(Vector2 point, Segment segment) noexcept {
    const Vector2 ab = segment.second - segment.first;
    const float abLengthSquared = ab.lengthSquared();
    if (abLengthSquared < 1e-6f) {
        return distanceBetween(point, segment.first);
    }
    const Vector2 ap = point - segment.first;
    const float t = std::clamp((ap.x * ab.x + ap.y * ab.y) / abLengthSquared, 0.0f, 1.0f);
    const Vector2 closest = segment.first + ab * t;
    return distanceBetween(point, closest);
}

// Index of the obstacle whose center lies closest to `segment` -- the
// heuristic's guess at "most likely responsible for this blockage."
std::size_t closestObstacleToSegment(const std::vector<core::ObstacleDefinition>& obstacles, Segment segment) {
    std::size_t bestIndex = 0;
    float bestDistance = pointToSegmentDistance(obstacles[0].position, segment);
    for (std::size_t i = 1; i < obstacles.size(); ++i) {
        const float candidateDistance = pointToSegmentDistance(obstacles[i].position, segment);
        if (candidateDistance < bestDistance) {
            bestDistance = candidateDistance;
            bestIndex = i;
        }
    }
    return bestIndex;
}

}  // namespace

RepairResult repairForReachability(MapDefinition map, float shipClearanceRadius, float cellSize, int maxRemovals) {
    RepairResult result;
    result.map = std::move(map);

    while (true) {
        const OccupancyGrid grid(result.map.arenaSize, result.map.obstacles, shipClearanceRadius, cellSize);
        const std::optional<Segment> failingSegment = findFirstUnsatisfiedSegment(result.map, grid);
        if (!failingSegment) {
            result.reachabilityRestored = true;
            return result;
        }
        if (result.obstaclesRemoved >= maxRemovals || result.map.obstacles.empty()) {
            result.reachabilityRestored = false;
            return result;
        }

        const std::size_t victimIndex = closestObstacleToSegment(result.map.obstacles, *failingSegment);
        result.map.obstacles.erase(result.map.obstacles.begin() + static_cast<std::ptrdiff_t>(victimIndex));
        ++result.obstaclesRemoved;
    }
}

}  // namespace astroparty::procgen
