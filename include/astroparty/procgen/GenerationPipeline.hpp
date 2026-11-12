#pragma once

#include <cstdint>
#include <vector>

#include "astroparty/core/MapDefinition.hpp"
#include "astroparty/procgen/MapGeneratorConfig.hpp"

namespace astroparty::procgen {

// Tallies of every outcome the generation loop can produce, kept separate
// so raw generator quality and post-repair usefulness are never conflated
// (see the Milestone 4 spec's "do not hide invalid maps from the
// statistics"):
//
//   totalAttempts   == structurallyInvalid + geometricallyInvalid + validRaw
//   accepted        == validRaw + repaired
//   rejected        == every invalid candidate repair did not fix
//                       (structurallyInvalid candidates are never repaired
//                       at all -- see GenerationPipeline.cpp)
struct GenerationStats {
    int totalAttempts = 0;
    int structurallyInvalid = 0;   // Failed MapDefinition::validate() or validateTeamResourceBattleStructure().
    int geometricallyInvalid = 0;  // Structurally fine, but checkGeometricReachability() failed.
    int validRaw = 0;              // Passed both checks with no repair needed.
    int repaired = 0;              // Was geometrically invalid; repairForReachability() fixed it and it was accepted.
    int rejected = 0;              // Invalid and not fixed; discarded.
    int accepted = 0;              // validRaw + repaired == the number of maps actually returned.

    // Fraction of all attempts that were valid with NO repair -- the
    // generator's raw quality, independent of the repair step.
    [[nodiscard]] float rawValidityRate() const noexcept {
        return totalAttempts > 0 ? static_cast<float>(validRaw) / static_cast<float>(totalAttempts) : 0.0f;
    }

    // Fraction of all attempts that ended up accepted (raw-valid or
    // repaired) -- how useful the generator is once repair is allowed to
    // help, which is always >= rawValidityRate().
    [[nodiscard]] float postRepairAcceptanceRate() const noexcept {
        return totalAttempts > 0 ? static_cast<float>(accepted) / static_cast<float>(totalAttempts) : 0.0f;
    }
};

// One accepted map plus the provenance needed to reproduce it and to know
// whether repair touched it.
struct GeneratedMapRecord {
    core::MapDefinition map;
    std::uint32_t seed = 0;
    bool wasRepaired = false;
    int obstaclesRemovedByRepair = 0;
};

struct GenerationResult {
    std::vector<GeneratedMapRecord> maps;
    GenerationStats stats;
};

// Repeatedly generates a candidate (one per seed, starting at `startSeed`
// and incrementing by one each attempt) and runs it through:
//
//   generate -> structural validity -> geometric reachability
//       -> accept (if both passed) / repair (if only geometric failed) ->
//          accept (if repair fixed it) or reject
//
// until EITHER `targetAcceptedCount` maps have been accepted OR
// `maxAttempts` candidates have been tried, whichever comes first -- this
// function never loops unboundedly, and always returns whatever it has
// managed to accept so far (possibly fewer than `targetAcceptedCount`) if
// the attempt budget runs out first.
//
// A structurally invalid candidate is always rejected outright, never
// repaired: see MapRepair.hpp's scope (it only ever removes obstacles to
// fix geometric disconnection) and the generator's own contract
// (generateCandidate() practically never produces a structurally invalid
// map, since home/spawn placement is a fixed formula -- a structural
// failure here would indicate a config edge case worth investigating, not
// something to paper over with a repair heuristic).
[[nodiscard]] GenerationResult generateMaps(const MapGeneratorConfig& config, std::uint32_t startSeed,
                                             int targetAcceptedCount, int maxAttempts, float shipClearanceRadius,
                                             float cellSize, int maxRepairRemovals);

}  // namespace astroparty::procgen
