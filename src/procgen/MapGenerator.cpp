#include "astroparty/procgen/MapGenerator.hpp"

#include <algorithm>
#include <cmath>
#include <vector>

namespace astroparty::procgen {

using core::HomeRegion;
using core::MapDefinition;
using core::ObstacleDefinition;
using core::ShipSpawnPoint;
using core::Vector2;

namespace {

// Ships always start facing the contested middle of the arena: team 0 (on
// the left) faces +X, team 1 (on the right) faces -X, matching the fixed
// team_arena.json's convention and this project's screen-angle convention
// (Vector2::fromAngleDegrees: 0 = up, clockwise-increasing).
constexpr float kTeam0HeadingDegrees = 90.0f;
constexpr float kTeam1HeadingDegrees = 270.0f;

float distance(Vector2 a, Vector2 b) noexcept { return std::sqrt(core::distanceSquared(a, b)); }

bool farEnoughFromPoint(Vector2 candidate, float candidateRadius, Vector2 other, float otherRadius,
                         float minGap) noexcept {
    return distance(candidate, other) >= candidateRadius + otherRadius + minGap;
}

// True if placing a circle of `radius` at `position` would respect
// `minGap` clearance from every obstacle already placed, every ship spawn
// point, and the required base separation from every home region.
bool isObstacleCandidateAcceptable(Vector2 position, float radius, const MapGeneratorConfig& config,
                                    const std::vector<ObstacleDefinition>& placedObstacles,
                                    const std::vector<ShipSpawnPoint>& shipSpawns,
                                    const std::vector<HomeRegion>& homeRegions) noexcept {
    if (position.x < radius || position.x > config.arenaSize.x - radius || position.y < radius ||
        position.y > config.arenaSize.y - radius) {
        return false;
    }
    for (const ObstacleDefinition& other : placedObstacles) {
        if (!farEnoughFromPoint(position, radius, other.position, other.radius, config.minObstacleSpacing)) {
            return false;
        }
    }
    for (const ShipSpawnPoint& spawn : shipSpawns) {
        if (!farEnoughFromPoint(position, radius, spawn.position, config.shipCollisionRadius,
                                 config.minObstacleSpacing)) {
            return false;
        }
    }
    for (const HomeRegion& home : homeRegions) {
        if (!farEnoughFromPoint(position, radius, home.position, home.radius, config.minBaseSeparationFromHome)) {
            return false;
        }
    }
    return true;
}

bool isResourceCandidateAcceptable(Vector2 position, const MapGeneratorConfig& config,
                                    const std::vector<ObstacleDefinition>& obstacles,
                                    const std::vector<ShipSpawnPoint>& shipSpawns,
                                    const std::vector<HomeRegion>& homeRegions,
                                    const std::vector<Vector2>& placedResources) noexcept {
    constexpr float kResourceClearance = 0.0f;  // A resource is a point; clearance comes from the other side's radius.
    if (position.x < 0.0f || position.x > config.arenaSize.x || position.y < 0.0f || position.y > config.arenaSize.y) {
        return false;
    }
    for (const ObstacleDefinition& obstacle : obstacles) {
        if (!farEnoughFromPoint(position, kResourceClearance, obstacle.position, obstacle.radius,
                                 config.shipCollisionRadius)) {
            return false;
        }
    }
    for (const ShipSpawnPoint& spawn : shipSpawns) {
        if (!farEnoughFromPoint(position, kResourceClearance, spawn.position, config.shipCollisionRadius, 0.0f)) {
            return false;
        }
    }
    for (const HomeRegion& home : homeRegions) {
        if (!farEnoughFromPoint(position, kResourceClearance, home.position, home.radius,
                                 config.minBaseSeparationFromHome)) {
            return false;
        }
    }
    for (const Vector2& other : placedResources) {
        if (distance(position, other) < config.shipCollisionRadius * 2.0f) {
            return false;
        }
    }
    return true;
}

// The X range cluster centers (or, when not clustering, individual
// obstacles) may be sampled from: a band of the requested width centered
// on the arena's vertical midline, clamped to the left half when
// `restrictToLeftHalf` is set (symmetric generation only ever samples the
// left half directly; the right half is produced by mirroring).
struct HorizontalBand {
    float minX;
    float maxX;
};

HorizontalBand contestedHorizontalBand(const MapGeneratorConfig& config, float marginX, float bandWidthFraction,
                                        bool restrictToLeftHalf) noexcept {
    const float midX = config.arenaSize.x / 2.0f;
    const float halfSpan = bandWidthFraction * (midX - marginX);
    HorizontalBand band{midX - halfSpan, midX + halfSpan};
    if (restrictToLeftHalf) {
        band.maxX = std::min(band.maxX, midX);
    }
    return band;
}

// Appends one obstacle at `position` (and, when `config.symmetric`, its
// mirror image across the vertical midline) if both positions are
// acceptable; silently does nothing otherwise, leaving the caller's retry
// loop to try a different candidate.
bool tryPlaceObstacle(Vector2 position, float radius, const MapGeneratorConfig& config,
                       std::vector<ObstacleDefinition>& obstacles, const std::vector<ShipSpawnPoint>& shipSpawns,
                       const std::vector<HomeRegion>& homeRegions) {
    if (!isObstacleCandidateAcceptable(position, radius, config, obstacles, shipSpawns, homeRegions)) {
        return false;
    }
    if (!config.symmetric) {
        obstacles.push_back(ObstacleDefinition{position, radius});
        return true;
    }

    const Vector2 mirrored{config.arenaSize.x - position.x, position.y};
    const bool onMidline = std::abs(position.x - config.arenaSize.x / 2.0f) < 1e-3f;
    if (!onMidline) {
        // `position` has not been added to `obstacles` yet, so neither of
        // the two isObstacleCandidateAcceptable calls above/below would
        // ever catch the mirrored pair colliding with EACH OTHER -- which
        // happens whenever a cluster lands close enough to the midline
        // that a point and its own mirror image overlap. Check that
        // directly before accepting the pair.
        if (!farEnoughFromPoint(position, radius, mirrored, radius, config.minObstacleSpacing)) {
            return false;
        }
        if (!isObstacleCandidateAcceptable(mirrored, radius, config, obstacles, shipSpawns, homeRegions)) {
            return false;
        }
    }
    obstacles.push_back(ObstacleDefinition{position, radius});
    if (!onMidline) {
        obstacles.push_back(ObstacleDefinition{mirrored, radius});
    }
    return true;
}

bool tryPlaceResource(Vector2 position, const MapGeneratorConfig& config, std::vector<Vector2>& resources,
                       const std::vector<ObstacleDefinition>& obstacles, const std::vector<ShipSpawnPoint>& shipSpawns,
                       const std::vector<HomeRegion>& homeRegions) {
    if (!isResourceCandidateAcceptable(position, config, obstacles, shipSpawns, homeRegions, resources)) {
        return false;
    }
    if (!config.symmetric) {
        resources.push_back(position);
        return true;
    }

    const Vector2 mirrored{config.arenaSize.x - position.x, position.y};
    const bool onMidline = std::abs(position.x - config.arenaSize.x / 2.0f) < 1e-3f;
    if (!onMidline) {
        // Same self-mirror-collision concern as tryPlaceObstacle above:
        // `position` is not yet in `resources`, so check the pair directly.
        if (distance(position, mirrored) < config.shipCollisionRadius * 2.0f) {
            return false;
        }
        if (!isResourceCandidateAcceptable(mirrored, config, obstacles, shipSpawns, homeRegions, resources)) {
            return false;
        }
    }
    resources.push_back(position);
    if (!onMidline) {
        resources.push_back(mirrored);
    }
    return true;
}

std::vector<ShipSpawnPoint> buildShipSpawns(const MapGeneratorConfig& config, float marginX) {
    std::vector<ShipSpawnPoint> spawns;
    spawns.reserve(static_cast<std::size_t>(config.shipsPerTeam) * 2);
    const float team0X = marginX;
    const float team1X = config.arenaSize.x - marginX;
    for (int i = 0; i < config.shipsPerTeam; ++i) {
        const float y = config.arenaSize.y * static_cast<float>(i + 1) / static_cast<float>(config.shipsPerTeam + 1);
        spawns.push_back(ShipSpawnPoint{Vector2{team0X, y}, kTeam0HeadingDegrees, 0});
    }
    for (int i = 0; i < config.shipsPerTeam; ++i) {
        const float y = config.arenaSize.y * static_cast<float>(i + 1) / static_cast<float>(config.shipsPerTeam + 1);
        spawns.push_back(ShipSpawnPoint{Vector2{team1X, y}, kTeam1HeadingDegrees, 1});
    }
    return spawns;
}

std::vector<HomeRegion> buildHomeRegions(const MapGeneratorConfig& config, float homeMarginX) {
    return {HomeRegion{0, Vector2{homeMarginX, config.arenaSize.y / 2.0f}, config.homeRegionRadius},
            HomeRegion{1, Vector2{config.arenaSize.x - homeMarginX, config.arenaSize.y / 2.0f}, config.homeRegionRadius}};
}

}  // namespace

MapDefinition generateCandidate(const MapGeneratorConfig& config, Rng& rng) {
    MapDefinition map;
    map.name = "generated";
    map.arenaSize = config.arenaSize;

    const float homeMarginX = config.baseMarginFraction * config.arenaSize.x;
    // Ship spawns sit just outside their own home region, with enough room
    // for the ship's own collision radius -- derived from already-
    // configured quantities rather than a second independent magic fraction.
    const float spawnMarginX = homeMarginX + config.homeRegionRadius + config.shipCollisionRadius * 3.0f;

    map.homeRegions = buildHomeRegions(config, homeMarginX);
    map.shipSpawns = buildShipSpawns(config, spawnMarginX);

    const float obstacleMarginY = config.arenaSize.y * 0.05f;
    const HorizontalBand obstacleBand =
        contestedHorizontalBand(config, spawnMarginX, config.clusterCenterBandWidthFraction, config.symmetric);

    std::vector<Vector2> clusterCenters;
    if (config.clusterCount > 0) {
        clusterCenters.reserve(static_cast<std::size_t>(config.clusterCount));
        for (int i = 0; i < config.clusterCount; ++i) {
            clusterCenters.push_back(
                rng.uniformInRectangle(Vector2{obstacleBand.minX, obstacleMarginY},
                                       Vector2{obstacleBand.maxX, config.arenaSize.y - obstacleMarginY}));
        }
    }

    // When generating only the left half for a symmetric map, request half
    // as many obstacles there (the mirror doubles them back up); an odd
    // requested count places one extra, unmirrored obstacle on the midline.
    const int obstaclesToPlace = config.symmetric ? (config.obstacleCount + 1) / 2 : config.obstacleCount;
    for (int i = 0; i < obstaclesToPlace; ++i) {
        bool placed = false;
        for (int attempt = 0; attempt < config.maxPlacementAttemptsPerObject && !placed; ++attempt) {
            const float radius = rng.uniformFloat(config.obstacleRadiusMin, config.obstacleRadiusMax);
            Vector2 position;
            if (config.clusterCount > 0) {
                const Vector2& center = clusterCenters[static_cast<std::size_t>(i) % clusterCenters.size()];
                position = center + rng.gaussianOffset(config.clusterSpreadRadius);
            } else {
                position = rng.uniformInRectangle(Vector2{obstacleBand.minX, obstacleMarginY},
                                                    Vector2{obstacleBand.maxX, config.arenaSize.y - obstacleMarginY});
            }
            placed = tryPlaceObstacle(position, radius, config, map.obstacles, map.shipSpawns, map.homeRegions);
        }
        // If every attempt failed, this one obstacle is simply omitted --
        // see MapGenerator.hpp's documented contract.
    }

    const Vector2 resourceRegionMin{spawnMarginX, config.arenaSize.y * 0.05f};
    // Symmetric generation samples only the left half directly (clamped at
    // the midline) and mirrors each point to build the right half, exactly
    // like the obstacle band above -- sampling the full width and then
    // clamping x after the fact would instead collapse every right-half
    // sample onto the midline itself, badly biasing the distribution.
    const float resourceRegionMaxX =
        config.symmetric ? config.arenaSize.x / 2.0f : config.arenaSize.x - spawnMarginX;
    const Vector2 resourceRegionMax{resourceRegionMaxX, config.arenaSize.y * 0.95f};
    const int resourcesToPlace = config.symmetric ? (config.resourceCount + 1) / 2 : config.resourceCount;
    for (int i = 0; i < resourcesToPlace; ++i) {
        bool placed = false;
        for (int attempt = 0; attempt < config.maxPlacementAttemptsPerObject && !placed; ++attempt) {
            const Vector2 position = rng.uniformInRectangle(resourceRegionMin, resourceRegionMax);
            placed = tryPlaceResource(position, config, map.resourceSpawnPoints, map.obstacles, map.shipSpawns,
                                       map.homeRegions);
        }
    }

    return map;
}

}  // namespace astroparty::procgen
