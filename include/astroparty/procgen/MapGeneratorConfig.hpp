#pragma once

#include <filesystem>

#include "astroparty/core/Vector2.hpp"

namespace astroparty::procgen {

// Every tunable parameter of the procedural Team Resource Battle map
// generator (see MapGenerator.hpp), grouped the same way SimulationConfig
// groups simulation parameters: a match's/map's rules are fully determined
// by one explicit, inspectable value rather than scattered constants.
//
// The three "map families" the Milestone 4 spec asks for (open, clustered,
// chokepoint) are NOT three different code paths -- they are three
// different values of this one config, documented alongside each field
// below and shipped as example JSON presets under configs/procgen/.
struct MapGeneratorConfig {
    core::Vector2 arenaSize{1280.0f, 720.0f};

    // How many ships per team spawn, and the ship's collision radius (must
    // match the SimulationConfig a generated map will actually be played
    // with, so clearance checks here agree with what World will enforce).
    int shipsPerTeam = 3;
    float shipCollisionRadius = 18.0f;
    float homeRegionRadius = 80.0f;
    // Home regions and ship spawns sit this far in from the left/right
    // arena edges -- kept fixed/formulaic rather than randomized, since
    // Milestone 3's fixed team_arena.json already establishes this is a
    // sound layout and nothing in Milestone 4 asks for home/spawn placement
    // itself to vary.
    float baseMarginFraction = 0.03125f;  // 40 units at the default 1280-wide arena.

    int resourceCount = 5;

    // --- Obstacles ---------------------------------------------------------
    int obstacleCount = 10;
    float obstacleRadiusMin = 20.0f;
    float obstacleRadiusMax = 45.0f;

    // 0 = scatter every obstacle independently (an "open" arena); N >= 1 =
    // group obstacles into N clusters, each sampled around its own center
    // with gaussianOffset(clusterSpreadRadius) (a "clustered" arena).
    int clusterCount = 0;
    float clusterSpreadRadius = 70.0f;
    // Restricts where CLUSTER CENTERS may land, as a fraction of arena
    // width centered on the vertical midline: 1.0 = anywhere between the
    // two bases; a small value (e.g. 0.2) forces every cluster toward the
    // midline, which is what turns the same clustering mechanism into a
    // "chokepoint" arena (one or two tight obstacle walls between the
    // bases) without any separate chokepoint-specific code path. Ignored
    // when clusterCount == 0.
    float clusterCenterBandWidthFraction = 1.0f;

    // Minimum required gap between any two obstacles' edges, and between an
    // obstacle's edge and a ship spawn/home region's edge, beyond simply
    // not overlapping -- gives ships room to maneuver around an obstacle
    // instead of grazing it.
    float minObstacleSpacing = 10.0f;
    // Minimum distance any obstacle's edge must keep from either home
    // region's center ("base separation"): keeps the contested middle of
    // the map where obstacles actually matter distinct from a safe zone
    // immediately around each base.
    float minBaseSeparationFromHome = 150.0f;

    // Mirrors team 0's generated obstacles and half of the resources across
    // the vertical midline to build team 1's side, guaranteeing exact
    // structural symmetry rather than merely similar-looking randomness.
    // false generates both halves independently for a deliberately
    // asymmetric layout.
    bool symmetric = true;

    // Bounded retry budget for placing a single obstacle/resource inside
    // one candidate map (see MapGenerator.cpp): if no valid position is
    // found within this many samples, that one object is simply omitted
    // from the candidate rather than retried forever.
    int maxPlacementAttemptsPerObject = 200;

    // Throws std::invalid_argument, naming the offending field, for any
    // physically nonsensical value (non-positive arena/radius/counts,
    // obstacleRadiusMin > obstacleRadiusMax, an out-of-[0,1] band fraction,
    // a negative cluster count, ...).
    void validate() const;

    [[nodiscard]] static MapGeneratorConfig loadFromFile(const std::filesystem::path& path);
};

}  // namespace astroparty::procgen
