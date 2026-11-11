#pragma once

#include "astroparty/core/MapDefinition.hpp"

namespace astroparty::procgen {

struct RepairResult {
    core::MapDefinition map;
    bool reachabilityRestored = false;
    int obstaclesRemoved = 0;
};

// A deliberately simple repair strategy for a geometrically disconnected
// candidate map (see MapValidity.hpp): repeatedly finds whichever
// connectivity requirement is currently unsatisfied, removes the single
// obstacle lying closest to the straight line between the two points that
// need to connect, and rechecks -- up to `maxRemovals` obstacles. This is
// not path planning or a general repair solver; it is a bounded heuristic
// that removes however many obstacles alias sit in the most consequential
// way, which is intentionally the simplest thing that can plausibly work
// here (see the Milestone 4 spec: "Avoid excessive implementation
// complexity").
//
// Never modifies ship spawns, home regions, or resource spawn points --
// only obstacles are ever removed. Returns the (possibly unmodified) map
// and whether reachability was actually restored; a caller that gets
// `reachabilityRestored == false` back should reject the candidate rather
// than accept a map this function could not actually fix.
[[nodiscard]] RepairResult repairForReachability(core::MapDefinition map, float shipClearanceRadius, float cellSize,
                                                  int maxRemovals);

}  // namespace astroparty::procgen
