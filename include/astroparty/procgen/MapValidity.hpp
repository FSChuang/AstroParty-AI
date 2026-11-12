#pragma once

#include "astroparty/core/MapDefinition.hpp"

namespace astroparty::procgen {

// The result of the GEOMETRIC reachability check only -- see this header's
// top-level comment for why that is a narrower claim than "valid" or
// "playable."
//
// This project deliberately distinguishes three different notions of
// "is this map okay," and never lets one stand in for another:
//
//   1. STRUCTURAL validity: is the map shaped correctly? (right spawn/home
//      counts, nothing out of bounds or overlapping) -- already checked by
//      core::MapDefinition::validate() and
//      core::validateTeamResourceBattleStructure(), unchanged by Milestone
//      4 and not duplicated here.
//   2. GEOMETRIC reachability (this file): is there physically room for a
//      ship-sized circle to get from one important location to another,
//      modeled as a coarse occupancy grid (see OccupancyGrid.hpp)? This
//      says nothing about whether a ship can actually FLY the route.
//   3. EMPIRICAL AI playability: does the existing, unmodified rule-based
//      AI actually reach these locations in a real headless match? Ships
//      continuously accelerate and can only ever turn clockwise, so a
//      geometrically connected map is not proof a ship can execute the
//      route -- a narrow S-bend that a grid path crosses easily may be
//      un-flyable at this project's turn rate and speed. This layer is a
//      diagnostic run on ACCEPTED maps (see the Milestone 4 report's
//      "AI behavior limitations" section), never a gate inside
//      GenerationPipeline.hpp's accept/repair/reject decision -- a slow or
//      timed-out match must not be misread as "the map is broken."
struct GeometricValidityReport {
    bool team0SpawnsCanReachOwnHome = false;
    bool team1SpawnsCanReachOwnHome = false;
    bool team0HomeCanReachEveryResource = false;
    bool team1HomeCanReachEveryResource = false;

    [[nodiscard]] bool isFullyConnected() const noexcept {
        return team0SpawnsCanReachOwnHome && team1SpawnsCanReachOwnHome && team0HomeCanReachEveryResource &&
               team1HomeCanReachEveryResource;
    }
};

// Checks ONLY geometric reachability (notion 2 above) for a Team Resource
// Battle map: every ship spawn can reach its own team's home region, and
// every resource spawn point is reachable from both home regions (which,
// since grid connectivity is symmetric, also proves a return trip from the
// resource back home is geometrically possible).
//
// Assumes `map` already passed structural validation (exactly one home
// region per team, etc.) -- this function does not re-check that, and its
// result is meaningless for a structurally malformed map.
[[nodiscard]] GeometricValidityReport checkGeometricReachability(const core::MapDefinition& map,
                                                                   float shipClearanceRadius, float cellSize);

}  // namespace astroparty::procgen
