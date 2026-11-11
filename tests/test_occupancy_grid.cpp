#include <doctest/doctest.h>

#include <cmath>
#include <vector>

#include "astroparty/core/MapDefinition.hpp"
#include "astroparty/core/Vector2.hpp"
#include "astroparty/procgen/OccupancyGrid.hpp"

using astroparty::core::ObstacleDefinition;
using astroparty::core::Vector2;
using astroparty::procgen::OccupancyGrid;

TEST_SUITE("procgen::OccupancyGrid") {
    TEST_CASE("an empty arena is fully connected between any two interior points") {
        OccupancyGrid grid(Vector2{800.0f, 600.0f}, {}, /*clearanceRadius=*/20.0f, /*cellSize=*/20.0f);

        CHECK(grid.isReachable(Vector2{40.0f, 40.0f}, Vector2{760.0f, 560.0f}));
        REQUIRE(grid.shortestPathLength(Vector2{40.0f, 40.0f}, Vector2{760.0f, 560.0f}).has_value());
    }

    TEST_CASE("a single obstacle in the middle does not disconnect the arena") {
        const std::vector<ObstacleDefinition> obstacles{{Vector2{400.0f, 300.0f}, 50.0f}};
        OccupancyGrid grid(Vector2{800.0f, 600.0f}, obstacles, 20.0f, 20.0f);

        CHECK(grid.isReachable(Vector2{40.0f, 40.0f}, Vector2{760.0f, 560.0f}));
    }

    TEST_CASE("a full-height wall of obstacles disconnects left from right") {
        std::vector<ObstacleDefinition> wall;
        for (float y = 0.0f; y <= 600.0f; y += 30.0f) {
            wall.push_back(ObstacleDefinition{Vector2{400.0f, y}, 40.0f});
        }
        OccupancyGrid grid(Vector2{800.0f, 600.0f}, wall, 20.0f, 20.0f);

        CHECK_FALSE(grid.isReachable(Vector2{40.0f, 300.0f}, Vector2{760.0f, 300.0f}));
    }

    TEST_CASE("shortest path length is longer when a detour around an obstacle is required") {
        const std::vector<ObstacleDefinition> noObstacles{};
        OccupancyGrid openGrid(Vector2{800.0f, 600.0f}, noObstacles, 15.0f, 15.0f);
        const auto directLength = openGrid.shortestPathLength(Vector2{100.0f, 300.0f}, Vector2{700.0f, 300.0f});
        REQUIRE(directLength.has_value());

        // A large obstacle directly on the straight-line path forces a detour.
        const std::vector<ObstacleDefinition> blocking{{Vector2{400.0f, 300.0f}, 150.0f}};
        OccupancyGrid blockedGrid(Vector2{800.0f, 600.0f}, blocking, 15.0f, 15.0f);
        const auto detourLength = blockedGrid.shortestPathLength(Vector2{100.0f, 300.0f}, Vector2{700.0f, 300.0f});
        REQUIRE(detourLength.has_value());

        CHECK(*detourLength > *directLength);
    }

    TEST_CASE("reachableFreeSpaceFraction is 1.0 for a fully open arena") {
        OccupancyGrid grid(Vector2{400.0f, 400.0f}, {}, 15.0f, 20.0f);
        CHECK(grid.reachableFreeSpaceFraction(Vector2{50.0f, 50.0f}) == doctest::Approx(1.0f));
    }

    TEST_CASE("reachableFreeSpaceFraction drops when obstacles wall off an island of free space") {
        // A ring of obstacles sealing off a pocket in one corner.
        std::vector<ObstacleDefinition> ring;
        for (float angleDegrees = 0.0f; angleDegrees < 360.0f; angleDegrees += 20.0f) {
            const float radians = angleDegrees * 3.14159265f / 180.0f;
            ring.push_back(ObstacleDefinition{Vector2{150.0f + 80.0f * std::cos(radians), 150.0f + 80.0f * std::sin(radians)}, 25.0f});
        }
        OccupancyGrid grid(Vector2{800.0f, 600.0f}, ring, 15.0f, 15.0f);

        const float fractionFromOutside = grid.reachableFreeSpaceFraction(Vector2{700.0f, 500.0f});
        CHECK(fractionFromOutside < 1.0f);
    }

    TEST_CASE("isPositionUsable is false for a position deep inside an obstacle") {
        const std::vector<ObstacleDefinition> obstacles{{Vector2{400.0f, 300.0f}, 100.0f}};
        OccupancyGrid grid(Vector2{800.0f, 600.0f}, obstacles, 15.0f, 15.0f);

        CHECK_FALSE(grid.isPositionUsable(Vector2{400.0f, 300.0f}));
    }

    TEST_CASE("isPositionUsable is true for a position well away from any obstacle or boundary") {
        OccupancyGrid grid(Vector2{800.0f, 600.0f}, {}, 15.0f, 15.0f);
        CHECK(grid.isPositionUsable(Vector2{400.0f, 300.0f}));
    }
}
