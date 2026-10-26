#include <doctest/doctest.h>

#include "astroparty/ai/Steering.hpp"
#include "astroparty/core/Vector2.hpp"

using astroparty::core::ObstacleDefinition;
using astroparty::core::Vector2;

namespace {
constexpr float kEpsilon = 0.01f;
}

TEST_SUITE("ai::headingToward") {
    TEST_CASE("matches Vector2::fromAngleDegrees's convention for the four cardinal directions") {
        const Vector2 origin{0.0f, 0.0f};
        CHECK(astroparty::ai::headingToward(origin, Vector2{0.0f, -100.0f}) == doctest::Approx(0.0f).epsilon(kEpsilon));
        CHECK(astroparty::ai::headingToward(origin, Vector2{100.0f, 0.0f}) == doctest::Approx(90.0f).epsilon(kEpsilon));
        CHECK(astroparty::ai::headingToward(origin, Vector2{0.0f, 100.0f}) == doctest::Approx(180.0f).epsilon(kEpsilon));
        CHECK(astroparty::ai::headingToward(origin, Vector2{-100.0f, 0.0f}) == doctest::Approx(270.0f).epsilon(kEpsilon));
    }

    TEST_CASE("is well-defined (does not crash or return NaN) for coincident points") {
        const float heading = astroparty::ai::headingToward(Vector2{5.0f, 5.0f}, Vector2{5.0f, 5.0f});
        CHECK(std::isfinite(heading));
    }
}

TEST_SUITE("ai::clockwiseRotationNeeded") {
    TEST_CASE("is zero when already facing the target") {
        CHECK(astroparty::ai::clockwiseRotationNeeded(90.0f, 90.0f) == doctest::Approx(0.0f));
    }

    TEST_CASE("is the positive clockwise distance between two headings") {
        CHECK(astroparty::ai::clockwiseRotationNeeded(0.0f, 90.0f) == doctest::Approx(90.0f));
        CHECK(astroparty::ai::clockwiseRotationNeeded(350.0f, 10.0f) == doctest::Approx(20.0f));
    }

    TEST_CASE("wraps correctly when the target is almost a full turn away") {
        CHECK(astroparty::ai::clockwiseRotationNeeded(10.0f, 0.0f) == doctest::Approx(350.0f));
    }
}

TEST_SUITE("ai::shouldTurnToFace") {
    TEST_CASE("returns false when already aligned within tolerance") {
        CHECK_FALSE(astroparty::ai::shouldTurnToFace(90.0f, 92.0f, /*toleranceDegrees=*/5.0f));
    }

    TEST_CASE("returns false when just slightly past the target (aligned from the other side)") {
        CHECK_FALSE(astroparty::ai::shouldTurnToFace(92.0f, 90.0f, /*toleranceDegrees=*/5.0f));
    }

    TEST_CASE("returns true when far from the target heading") {
        CHECK(astroparty::ai::shouldTurnToFace(0.0f, 180.0f, /*toleranceDegrees=*/5.0f));
    }
}

TEST_SUITE("ai::isObstacleAhead") {
    TEST_CASE("detects an obstacle directly ahead within lookahead range") {
        const std::vector<ObstacleDefinition> obstacles{{Vector2{0.0f, -100.0f}, 20.0f}};
        // Heading 0 degrees points up (-Y) in this project's convention.
        CHECK(astroparty::ai::isObstacleAhead(Vector2{0.0f, 0.0f}, 0.0f, /*travelerRadius=*/10.0f, obstacles,
                                               /*lookaheadDistance=*/150.0f, /*clearance=*/5.0f));
    }

    TEST_CASE("ignores an obstacle behind the traveler") {
        const std::vector<ObstacleDefinition> obstacles{{Vector2{0.0f, 100.0f}, 20.0f}};  // Behind heading 0.
        CHECK_FALSE(astroparty::ai::isObstacleAhead(Vector2{0.0f, 0.0f}, 0.0f, 10.0f, obstacles, 150.0f, 5.0f));
    }

    TEST_CASE("ignores an obstacle beyond the lookahead distance") {
        const std::vector<ObstacleDefinition> obstacles{{Vector2{0.0f, -1000.0f}, 20.0f}};
        CHECK_FALSE(astroparty::ai::isObstacleAhead(Vector2{0.0f, 0.0f}, 0.0f, 10.0f, obstacles, 150.0f, 5.0f));
    }

    TEST_CASE("ignores an obstacle far off to the side of the straight-ahead path") {
        const std::vector<ObstacleDefinition> obstacles{{Vector2{500.0f, -50.0f}, 20.0f}};  // Far to the side.
        CHECK_FALSE(astroparty::ai::isObstacleAhead(Vector2{0.0f, 0.0f}, 0.0f, 10.0f, obstacles, 150.0f, 5.0f));
    }

    TEST_CASE("returns false when there are no obstacles at all") {
        CHECK_FALSE(astroparty::ai::isObstacleAhead(Vector2{0.0f, 0.0f}, 0.0f, 10.0f, {}, 150.0f, 5.0f));
    }
}
