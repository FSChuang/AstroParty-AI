#pragma once

#include "astroparty/core/MapDefinition.hpp"

namespace astroparty::procgen {

// The measurable properties of one GENERATED map used for the Milestone 4
// expressive-range analysis (see scripts/analyze_generated_maps.py). Every
// field here is computed from the map's actual, final geometry -- never
// from the MapGeneratorConfig that produced it -- so these numbers answer
// "what did the generator actually build," which can and does differ from
// the config's target values (obstacle placement can fail and be skipped;
// repair can remove obstacles; see MapGenerator.hpp/MapRepair.hpp).
struct MapMetrics {
    // Fraction of the arena's area covered by obstacles (sum of
    // obstacle areas / arena area). Independent of arena size, so it is
    // comparable across maps generated at different arenaSize values.
    float obstacleCoverageRatio = 0.0f;

    // Average shortest (grid-approximate) path length from each team's
    // home region to every resource spawn point, in world units -- see
    // OccupancyGrid::shortestPathLength(). An unreachable resource
    // contributes nothing to the average (should not occur on a map that
    // passed checkGeometricReachability(), but is handled defensively).
    float team0NavigationPathLength = 0.0f;
    float team1NavigationPathLength = 0.0f;

    // |team0NavigationPathLength - team1NavigationPathLength|: how much
    // easier one team's path to the contested resources is than the
    // other's. 0 for a perfectly balanced map; a symmetric map (see
    // MapGeneratorConfig::symmetric) should measure very close to 0 up to
    // grid-discretization noise.
    float teamAccessibilityImbalance = 0.0f;

    // Number of spatially separate obstacle clusters, derived purely from
    // the obstacles' actual positions (two obstacles are in the same
    // cluster if the gap between their edges is below
    // `clusterGapThreshold`) -- independent of, and a cross-check against,
    // MapGeneratorConfig::clusterCount (which only constrains where
    // cluster CENTERS were sampled from, not how the obstacles actually
    // ended up grouped after placement/repair).
    int obstacleClusterCount = 0;

    // Fraction of all free (non-blocked) grid cells reachable from team
    // 0's home region. 1.0 means the open space is one connected region;
    // lower values mean obstacles have walled off part of the arena into
    // pockets that are open but unreachable from where the match actually
    // happens -- a condition checkGeometricReachability() does not itself
    // flag, since it only checks the specific required paths, not whether
    // every last open cell is reachable.
    float reachableFreeSpaceFraction = 0.0f;
};

// Computes every field of MapMetrics for `map`. `shipClearanceRadius` and
// `cellSize` parameterize the OccupancyGrid exactly as they do for
// checkGeometricReachability(); `clusterGapThreshold` is the maximum gap
// (in world units) between two obstacles' edges for them to count as the
// same spatial cluster.
[[nodiscard]] MapMetrics computeMapMetrics(const core::MapDefinition& map, float shipClearanceRadius, float cellSize,
                                            float clusterGapThreshold);

}  // namespace astroparty::procgen
