#include "astroparty/procgen/MapValidity.hpp"

#include <optional>

#include "astroparty/procgen/OccupancyGrid.hpp"

namespace astroparty::procgen {

using core::HomeRegion;
using core::MapDefinition;
using core::ShipSpawnPoint;
using core::Vector2;

namespace {

std::optional<Vector2> homePositionForTeam(const MapDefinition& map, int teamId) {
    for (const HomeRegion& home : map.homeRegions) {
        if (home.teamId == teamId) {
            return home.position;
        }
    }
    return std::nullopt;
}

bool allSpawnsOfTeamCanReach(const MapDefinition& map, int teamId, Vector2 destination, const OccupancyGrid& grid) {
    bool anySpawnForTeam = false;
    for (const ShipSpawnPoint& spawn : map.shipSpawns) {
        if (spawn.teamId != teamId) {
            continue;
        }
        anySpawnForTeam = true;
        if (!grid.isReachable(spawn.position, destination)) {
            return false;
        }
    }
    return anySpawnForTeam;  // Vacuously "cannot reach" if the team has no spawns at all.
}

bool homeCanReachEveryResource(Vector2 homePosition, const MapDefinition& map, const OccupancyGrid& grid) {
    if (map.resourceSpawnPoints.empty()) {
        return false;  // Nothing to deliver is not a meaningful Team Resource Battle map.
    }
    for (const Vector2& resource : map.resourceSpawnPoints) {
        if (!grid.isReachable(homePosition, resource)) {
            return false;
        }
    }
    return true;
}

}  // namespace

GeometricValidityReport checkGeometricReachability(const MapDefinition& map, float shipClearanceRadius,
                                                     float cellSize) {
    const OccupancyGrid grid(map.arenaSize, map.obstacles, shipClearanceRadius, cellSize);

    GeometricValidityReport report;
    const std::optional<Vector2> home0 = homePositionForTeam(map, 0);
    const std::optional<Vector2> home1 = homePositionForTeam(map, 1);

    if (home0) {
        report.team0SpawnsCanReachOwnHome = allSpawnsOfTeamCanReach(map, 0, *home0, grid);
        report.team0HomeCanReachEveryResource = homeCanReachEveryResource(*home0, map, grid);
    }
    if (home1) {
        report.team1SpawnsCanReachOwnHome = allSpawnsOfTeamCanReach(map, 1, *home1, grid);
        report.team1HomeCanReachEveryResource = homeCanReachEveryResource(*home1, map, grid);
    }
    return report;
}

}  // namespace astroparty::procgen
