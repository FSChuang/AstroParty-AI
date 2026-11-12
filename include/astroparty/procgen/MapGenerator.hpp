#pragma once

#include "astroparty/core/MapDefinition.hpp"
#include "astroparty/procgen/MapGeneratorConfig.hpp"
#include "astroparty/procgen/Rng.hpp"

namespace astroparty::procgen {

// Produces one candidate Team Resource Battle MapDefinition from `config`,
// drawing all randomness from `rng` (which the caller owns and seeds
// explicitly -- see Rng.hpp). The same config and the same Rng state
// (i.e. the same seed, if `rng` was freshly constructed for this call)
// always produce the same candidate.
//
// Home regions and ship spawn points are placed by a fixed formula (one per
// team, mirrored across the arena, matching the existing fixed
// team_arena.json's layout convention) -- nothing about Milestone 4 asks
// base/spawn placement itself to vary, only obstacles and resources do.
// Obstacles are placed either as an unconstrained uniform scatter
// (config.clusterCount == 0) or grouped into config.clusterCount clusters
// sampled with a Gaussian offset around each cluster's center
// (config.clusterCount >= 1); constraining where cluster CENTERS may land
// (config.clusterCenterBandWidthFraction) is what turns the same mechanism
// into either a "clustered" or a "chokepoint" arena -- see
// MapGeneratorConfig.hpp. Resources are scattered across the contested
// middle of the arena, clear of both home regions.
//
// Rejection sampling places each individual obstacle/resource: a candidate
// position is resampled (new position, new radius where applicable) up to
// config.maxPlacementAttemptsPerObject times against every already-placed
// object (other obstacles/resources, ship spawns, home regions) and the
// arena boundary; if none of those attempts succeeds, that one object is
// simply omitted from the result rather than retried forever -- so the
// returned map may contain fewer obstacles/resources than requested when
// `config` is tightly constrained. Separately, when config.symmetric is
// true, an ODD obstacleCount/resourceCount is effectively rounded UP to the
// next even number (each placement attempt mirrors into a pair; landing
// exactly on the vertical midline, the only way to get an unmirrored
// single object, has probability zero under continuous sampling) --
// requesting an even count avoids this if an exact count matters. This
// function never throws for a
// structurally valid config and never loops unboundedly; it does NOT
// guarantee the result is itself a valid map (see MapValidity.hpp for that)
// -- that judgment, and any whole-candidate repair/rejection, belongs to
// GenerationPipeline.hpp, not to this function.
[[nodiscard]] core::MapDefinition generateCandidate(const MapGeneratorConfig& config, Rng& rng);

}  // namespace astroparty::procgen
