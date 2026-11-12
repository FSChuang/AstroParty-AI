#include <doctest/doctest.h>

#include <cmath>

#include "astroparty/core/MapDefinition.hpp"
#include "astroparty/procgen/MapGenerator.hpp"
#include "astroparty/procgen/Rng.hpp"

using astroparty::core::MapDefinition;
using astroparty::core::ObstacleDefinition;
using astroparty::core::ShipSpawnPoint;
using astroparty::core::Vector2;
using astroparty::procgen::generateCandidate;
using astroparty::procgen::MapGeneratorConfig;
using astroparty::procgen::Rng;

namespace {

float distance(Vector2 a, Vector2 b) { return std::sqrt(astroparty::core::distanceSquared(a, b)); }

bool anyObstacleOverlapsAnother(const MapDefinition& map) {
    for (std::size_t i = 0; i < map.obstacles.size(); ++i) {
        for (std::size_t j = i + 1; j < map.obstacles.size(); ++j) {
            if (distance(map.obstacles[i].position, map.obstacles[j].position) <
                map.obstacles[i].radius + map.obstacles[j].radius) {
                return true;
            }
        }
    }
    return false;
}

bool anyObstacleOverlapsShipSpawn(const MapDefinition& map, float shipCollisionRadius) {
    for (const ObstacleDefinition& obstacle : map.obstacles) {
        for (const ShipSpawnPoint& spawn : map.shipSpawns) {
            if (distance(obstacle.position, spawn.position) < obstacle.radius + shipCollisionRadius) {
                return true;
            }
        }
    }
    return false;
}

}  // namespace

TEST_SUITE("procgen::generateCandidate determinism") {
    TEST_CASE("the same config and seed produce an identical map") {
        MapGeneratorConfig config;
        config.obstacleCount = 10;
        config.clusterCount = 2;

        Rng rngA(99);
        const MapDefinition mapA = generateCandidate(config, rngA);
        Rng rngB(99);
        const MapDefinition mapB = generateCandidate(config, rngB);

        CHECK(mapA.toJsonString() == mapB.toJsonString());
    }

    TEST_CASE("different seeds produce a different map") {
        MapGeneratorConfig config;
        config.obstacleCount = 10;
        config.clusterCount = 2;

        Rng rngA(1);
        const MapDefinition mapA = generateCandidate(config, rngA);
        Rng rngB(2);
        const MapDefinition mapB = generateCandidate(config, rngB);

        CHECK(mapA.toJsonString() != mapB.toJsonString());
    }
}

TEST_SUITE("procgen::generateCandidate structure") {
    TEST_CASE("produces a structurally valid Team Resource Battle map") {
        MapGeneratorConfig config;
        config.obstacleCount = 8;
        Rng rng(5);

        const MapDefinition map = generateCandidate(config, rng);

        CHECK_NOTHROW(map.validate());
        CHECK_NOTHROW(astroparty::core::validateTeamResourceBattleStructure(map));
    }

    TEST_CASE("places exactly shipsPerTeam spawns on each team") {
        MapGeneratorConfig config;
        config.shipsPerTeam = 3;
        Rng rng(5);

        const MapDefinition map = generateCandidate(config, rng);

        int team0Count = 0, team1Count = 0;
        for (const ShipSpawnPoint& spawn : map.shipSpawns) {
            if (spawn.teamId == 0) ++team0Count;
            if (spawn.teamId == 1) ++team1Count;
        }
        CHECK(team0Count == 3);
        CHECK(team1Count == 3);
    }

    TEST_CASE("every obstacle stays within the arena bounds") {
        MapGeneratorConfig config;
        config.obstacleCount = 15;
        config.clusterCount = 3;
        Rng rng(11);

        const MapDefinition map = generateCandidate(config, rng);

        for (const ObstacleDefinition& obstacle : map.obstacles) {
            CHECK(obstacle.position.x - obstacle.radius >= 0.0f);
            CHECK(obstacle.position.x + obstacle.radius <= map.arenaSize.x);
            CHECK(obstacle.position.y - obstacle.radius >= 0.0f);
            CHECK(obstacle.position.y + obstacle.radius <= map.arenaSize.y);
        }
    }

    TEST_CASE("no two obstacles overlap") {
        MapGeneratorConfig config;
        config.obstacleCount = 15;
        config.clusterCount = 3;
        config.clusterSpreadRadius = 50.0f;
        Rng rng(11);

        const MapDefinition map = generateCandidate(config, rng);

        CHECK_FALSE(anyObstacleOverlapsAnother(map));
    }

    TEST_CASE("no obstacle overlaps a ship spawn point") {
        MapGeneratorConfig config;
        config.obstacleCount = 15;
        Rng rng(11);

        const MapDefinition map = generateCandidate(config, rng);

        CHECK_FALSE(anyObstacleOverlapsShipSpawn(map, config.shipCollisionRadius));
    }

    TEST_CASE("no obstacle encroaches within minBaseSeparationFromHome of a home region") {
        MapGeneratorConfig config;
        config.obstacleCount = 20;
        config.minBaseSeparationFromHome = 100.0f;
        Rng rng(11);

        const MapDefinition map = generateCandidate(config, rng);

        for (const ObstacleDefinition& obstacle : map.obstacles) {
            for (const auto& home : map.homeRegions) {
                CHECK(distance(obstacle.position, home.position) >=
                      home.radius + config.minBaseSeparationFromHome + obstacle.radius);
            }
        }
    }

    TEST_CASE("symmetric generation mirrors every obstacle across the vertical midline") {
        MapGeneratorConfig config;
        config.obstacleCount = 10;
        config.symmetric = true;
        Rng rng(3);

        const MapDefinition map = generateCandidate(config, rng);

        REQUIRE(map.obstacles.size() % 2 == 0);
        for (std::size_t i = 0; i < map.obstacles.size(); i += 2) {
            const ObstacleDefinition& a = map.obstacles[i];
            const ObstacleDefinition& b = map.obstacles[i + 1];
            CHECK(a.position.x + b.position.x == doctest::Approx(map.arenaSize.x));
            CHECK(a.position.y == doctest::Approx(b.position.y));
            CHECK(a.radius == doctest::Approx(b.radius));
        }
    }

    TEST_CASE("asymmetric generation does not force a mirrored layout") {
        MapGeneratorConfig config;
        config.obstacleCount = 10;
        config.symmetric = false;
        Rng rng(3);

        const MapDefinition map = generateCandidate(config, rng);

        CHECK(map.obstacles.size() == 10);
    }

    TEST_CASE("an obstacleCount of zero produces an obstacle-free, still-valid map") {
        MapGeneratorConfig config;
        config.obstacleCount = 0;
        Rng rng(1);

        const MapDefinition map = generateCandidate(config, rng);

        CHECK(map.obstacles.empty());
        CHECK_NOTHROW(map.validate());
    }
}

TEST_SUITE("procgen::generateCandidate edge cases") {
    TEST_CASE("an impossibly dense request terminates and places fewer obstacles than requested, with no overlaps") {
        MapGeneratorConfig config;
        config.obstacleCount = 200;
        config.obstacleRadiusMin = 80.0f;
        config.obstacleRadiusMax = 100.0f;
        config.maxPlacementAttemptsPerObject = 50;
        Rng rng(5);

        const MapDefinition map = generateCandidate(config, rng);

        CHECK(map.obstacles.size() < 200);
        CHECK_FALSE(anyObstacleOverlapsAnother(map));
    }

    TEST_CASE("a minimal arena still produces a structurally valid map") {
        MapGeneratorConfig config;
        config.arenaSize = {400.0f, 300.0f};
        config.homeRegionRadius = 30.0f;
        config.minBaseSeparationFromHome = 20.0f;
        config.obstacleCount = 5;
        Rng rng(1);

        const MapDefinition map = generateCandidate(config, rng);

        CHECK_NOTHROW(map.validate());
        CHECK_NOTHROW(astroparty::core::validateTeamResourceBattleStructure(map));
    }

    TEST_CASE("a narrow chokepoint-style band configuration still produces a valid, non-overlapping map") {
        MapGeneratorConfig config;
        config.obstacleCount = 14;
        config.clusterCount = 2;
        config.clusterSpreadRadius = 35.0f;
        config.clusterCenterBandWidthFraction = 0.15f;
        Rng rng(123);

        const MapDefinition map = generateCandidate(config, rng);

        CHECK_NOTHROW(map.validate());
        CHECK_FALSE(anyObstacleOverlapsAnother(map));
    }

    TEST_CASE("resource spawn points stay within the arena and clear of obstacles") {
        MapGeneratorConfig config;
        config.obstacleCount = 10;
        config.resourceCount = 6;
        Rng rng(42);

        const MapDefinition map = generateCandidate(config, rng);

        for (const Vector2& resource : map.resourceSpawnPoints) {
            CHECK(resource.x >= 0.0f);
            CHECK(resource.x <= map.arenaSize.x);
            CHECK(resource.y >= 0.0f);
            CHECK(resource.y <= map.arenaSize.y);
            for (const ObstacleDefinition& obstacle : map.obstacles) {
                CHECK(distance(resource, obstacle.position) >= obstacle.radius);
            }
        }
    }
}
