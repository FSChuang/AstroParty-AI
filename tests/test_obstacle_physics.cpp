#include <doctest/doctest.h>

#include <cmath>

#include "astroparty/core/Vector2.hpp"
#include "astroparty/physics/Obstacle.hpp"

using astroparty::core::Vector2;
using astroparty::physics::circlesOverlap;
using astroparty::physics::pushOutOfCircle;

TEST_SUITE("physics::circlesOverlap") {
    TEST_CASE("two distant circles do not overlap") {
        CHECK_FALSE(circlesOverlap(Vector2{0.0f, 0.0f}, 10.0f, Vector2{1000.0f, 0.0f}, 10.0f));
    }

    TEST_CASE("two circles whose centers are closer than the sum of radii overlap") {
        CHECK(circlesOverlap(Vector2{0.0f, 0.0f}, 10.0f, Vector2{15.0f, 0.0f}, 10.0f));
    }

    TEST_CASE("two circles exactly tangent do not count as overlapping") {
        // distanceSquared == combinedRadius^2 is the exact boundary; the
        // implementation uses a strict less-than, matching
        // isPointWithinRadius's convention elsewhere in this project.
        CHECK_FALSE(circlesOverlap(Vector2{0.0f, 0.0f}, 10.0f, Vector2{20.0f, 0.0f}, 10.0f));
    }
}

TEST_SUITE("physics::pushOutOfCircle") {
    TEST_CASE("a position outside the obstacle is left unchanged") {
        const Vector2 position{100.0f, 0.0f};
        const Vector2 result = pushOutOfCircle(position, 10.0f, Vector2{0.0f, 0.0f}, 10.0f);
        CHECK(result == position);
    }

    TEST_CASE("an overlapping position is pushed to exactly the combined radius away") {
        const Vector2 obstaclePosition{0.0f, 0.0f};
        const Vector2 overlapping{5.0f, 0.0f};  // well inside combined radius 20
        const Vector2 result = pushOutOfCircle(overlapping, 10.0f, obstaclePosition, 10.0f);

        CHECK(result.x == doctest::Approx(20.0f));
        CHECK(result.y == doctest::Approx(0.0f));
    }

    TEST_CASE("pushing preserves the direction from the obstacle to the original position") {
        const Vector2 obstaclePosition{100.0f, 100.0f};
        const Vector2 overlapping{105.0f, 100.0f};  // directly to the +X side of the obstacle
        const Vector2 result = pushOutOfCircle(overlapping, 8.0f, obstaclePosition, 12.0f);

        CHECK(result.x == doctest::Approx(120.0f));  // 100 + (8+12)
        CHECK(result.y == doctest::Approx(100.0f));
    }

    TEST_CASE("a position exactly at the obstacle's center still produces a finite, valid result") {
        const Vector2 obstaclePosition{50.0f, 50.0f};
        const Vector2 result = pushOutOfCircle(obstaclePosition, 10.0f, obstaclePosition, 10.0f);

        CHECK(std::isfinite(result.x));
        CHECK(std::isfinite(result.y));
        // Must end up exactly combinedRadius away from the obstacle, in some direction.
        CHECK(astroparty::core::distanceSquared(result, obstaclePosition) == doctest::Approx(20.0f * 20.0f));
    }
}
