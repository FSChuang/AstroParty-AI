#include <doctest/doctest.h>

#include "astroparty/procgen/GenerationPipeline.hpp"
#include "astroparty/procgen/MapGenerator.hpp"
#include "astroparty/procgen/Rng.hpp"

using astroparty::procgen::generateMaps;
using astroparty::procgen::MapGeneratorConfig;

namespace {

constexpr float kShipClearanceRadius = 18.0f;
constexpr float kCellSize = 15.0f;

// Empirically verified (see commit history) to make geometric invalidity,
// successful repair, AND outright rejection all actually occur within a
// bounded attempt budget starting at seed 0 -- a near-solid scatter of
// obstacles packed almost arena-wide, with minimal spacing, makes most raw
// candidates seal off at least one required path.
MapGeneratorConfig denseRepairProneConfig() {
    MapGeneratorConfig config;
    config.obstacleCount = 80;
    config.clusterCount = 0;
    config.minObstacleSpacing = 1.0f;
    config.obstacleRadiusMin = 30.0f;
    config.obstacleRadiusMax = 45.0f;
    config.minBaseSeparationFromHome = 50.0f;
    return config;
}

}  // namespace

TEST_SUITE("procgen::generateMaps accounting") {
    TEST_CASE("a dense configuration exercises repair and rejection, with consistent accounting throughout") {
        // One shared (slower, ~1s) run backs several assertions instead of
        // re-running the same dense, repair-heavy generation per-assertion.
        const auto result = generateMaps(denseRepairProneConfig(), /*startSeed=*/0, /*targetAcceptedCount=*/3,
                                          /*maxAttempts=*/2000, kShipClearanceRadius, kCellSize,
                                          /*maxRepairRemovals=*/3);
        const auto& stats = result.stats;

        CHECK(stats.accepted == stats.validRaw + stats.repaired);
        CHECK(stats.totalAttempts == stats.accepted + stats.rejected);
        CHECK(stats.totalAttempts == stats.structurallyInvalid + stats.geometricallyInvalid + stats.validRaw);
        CHECK(static_cast<int>(result.maps.size()) == stats.accepted);

        CHECK(stats.repaired > 0);
        CHECK(stats.rejected > 0);
        CHECK(stats.accepted == 3);

        CHECK(stats.postRepairAcceptanceRate() >= stats.rawValidityRate());
    }

    TEST_CASE("a config with no obstacles has a perfect raw validity rate") {
        MapGeneratorConfig config;
        config.obstacleCount = 0;

        const auto result = generateMaps(config, 0, 5, 20, kShipClearanceRadius, kCellSize, 3);

        CHECK(result.stats.rawValidityRate() == doctest::Approx(1.0f));
        CHECK(result.stats.repaired == 0);
        CHECK(result.stats.rejected == 0);
    }
}

TEST_SUITE("procgen::generateMaps bounded termination") {
    TEST_CASE("stops at maxAttempts and honestly reports a shortfall instead of looping forever") {
        MapGeneratorConfig config;
        config.obstacleCount = 5;

        const auto result = generateMaps(config, 0, /*targetAcceptedCount=*/1000, /*maxAttempts=*/5,
                                          kShipClearanceRadius, kCellSize, 3);

        CHECK(result.stats.totalAttempts == 5);
        CHECK(static_cast<int>(result.maps.size()) < 1000);
        CHECK(static_cast<int>(result.maps.size()) == result.stats.accepted);
    }

    TEST_CASE("stops as soon as the target accepted count is reached, without using the full attempt budget") {
        MapGeneratorConfig config;
        config.obstacleCount = 5;  // Sparse enough that every candidate should validate raw.

        const auto result = generateMaps(config, 0, /*targetAcceptedCount=*/3, /*maxAttempts=*/1000,
                                          kShipClearanceRadius, kCellSize, 3);

        CHECK(result.maps.size() == 3);
        CHECK(result.stats.totalAttempts <= 1000);
    }
}

TEST_SUITE("procgen::generateMaps determinism and provenance") {
    TEST_CASE("the same config, start seed, and budgets produce identical results") {
        MapGeneratorConfig config;
        config.obstacleCount = 10;
        config.clusterCount = 2;

        const auto first = generateMaps(config, 42, 5, 50, kShipClearanceRadius, kCellSize, 3);
        const auto second = generateMaps(config, 42, 5, 50, kShipClearanceRadius, kCellSize, 3);

        REQUIRE(first.maps.size() == second.maps.size());
        for (std::size_t i = 0; i < first.maps.size(); ++i) {
            CHECK(first.maps[i].seed == second.maps[i].seed);
            CHECK(first.maps[i].map.toJsonString() == second.maps[i].map.toJsonString());
        }
        CHECK(first.stats.totalAttempts == second.stats.totalAttempts);
    }

    TEST_CASE("each accepted map's seed is reproducible: regenerating from that exact seed matches") {
        MapGeneratorConfig config;
        config.obstacleCount = 10;
        config.clusterCount = 2;

        const auto result = generateMaps(config, 100, 3, 50, kShipClearanceRadius, kCellSize, 3);

        REQUIRE_FALSE(result.maps.empty());
        for (const auto& record : result.maps) {
            if (record.wasRepaired) {
                continue;  // Repair mutates the map; only raw-valid records round-trip through the seed alone.
            }
            astroparty::procgen::Rng rng(record.seed);
            const auto regenerated = astroparty::procgen::generateCandidate(config, rng);
            CHECK(regenerated.toJsonString() == record.map.toJsonString());
        }
    }
}
