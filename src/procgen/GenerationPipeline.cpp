#include "astroparty/procgen/GenerationPipeline.hpp"

#include <stdexcept>

#include "astroparty/procgen/MapGenerator.hpp"
#include "astroparty/procgen/MapRepair.hpp"
#include "astroparty/procgen/MapValidity.hpp"
#include "astroparty/procgen/Rng.hpp"

namespace astroparty::procgen {

using core::MapDefinition;

namespace {

bool isStructurallyValid(const MapDefinition& map) {
    try {
        map.validate();
        core::validateTeamResourceBattleStructure(map);
        return true;
    } catch (const std::invalid_argument&) {
        return false;
    }
}

}  // namespace

GenerationResult generateMaps(const MapGeneratorConfig& config, std::uint32_t startSeed, int targetAcceptedCount,
                               int maxAttempts, float shipClearanceRadius, float cellSize, int maxRepairRemovals) {
    GenerationResult result;

    for (int attempt = 0; attempt < maxAttempts && static_cast<int>(result.maps.size()) < targetAcceptedCount;
         ++attempt) {
        const std::uint32_t seed = startSeed + static_cast<std::uint32_t>(attempt);
        Rng rng(seed);
        MapDefinition candidate = generateCandidate(config, rng);
        ++result.stats.totalAttempts;

        if (!isStructurallyValid(candidate)) {
            ++result.stats.structurallyInvalid;
            ++result.stats.rejected;
            continue;
        }

        const GeometricValidityReport report = checkGeometricReachability(candidate, shipClearanceRadius, cellSize);
        if (report.isFullyConnected()) {
            ++result.stats.validRaw;
            ++result.stats.accepted;
            result.maps.push_back(GeneratedMapRecord{std::move(candidate), seed, false, 0});
            continue;
        }

        ++result.stats.geometricallyInvalid;
        const RepairResult repaired =
            repairForReachability(candidate, shipClearanceRadius, cellSize, maxRepairRemovals);
        if (repaired.reachabilityRestored) {
            ++result.stats.repaired;
            ++result.stats.accepted;
            result.maps.push_back(GeneratedMapRecord{repaired.map, seed, true, repaired.obstaclesRemoved});
        } else {
            ++result.stats.rejected;
        }
    }

    return result;
}

}  // namespace astroparty::procgen
