#include <doctest/doctest.h>

#include "astroparty/core/Vector2.hpp"

using astroparty::core::Vector2;

namespace {
constexpr float kEpsilon = 0.001f;
}

TEST_SUITE("Vector2") {
    TEST_CASE("operator+ and operator- combine components") {
        const Vector2 a{3.0f, 4.0f};
        const Vector2 b{1.0f, 2.0f};
        CHECK((a + b) == Vector2{4.0f, 6.0f});
        CHECK((a - b) == Vector2{2.0f, 2.0f});
    }

    TEST_CASE("operator* scales both components") {
        const Vector2 v{2.0f, -3.0f};
        CHECK((v * 2.0f) == Vector2{4.0f, -6.0f});
    }

    TEST_CASE("lengthSquared and length of a 3-4-5 triangle") {
        const Vector2 v{3.0f, 4.0f};
        CHECK(v.lengthSquared() == doctest::Approx(25.0f));
        CHECK(v.length() == doctest::Approx(5.0f));
    }

    TEST_CASE("clampedToLength leaves a short vector unchanged") {
        const Vector2 v{1.0f, 0.0f};
        const Vector2 clamped = v.clampedToLength(10.0f);
        CHECK(clamped == v);
    }

    TEST_CASE("clampedToLength shortens a vector exceeding the limit, preserving direction") {
        const Vector2 v{6.0f, 8.0f};  // length 10
        const Vector2 clamped = v.clampedToLength(5.0f);
        CHECK(clamped.length() == doctest::Approx(5.0f));
        // Direction preserved: clamped is the original vector scaled by 1/2.
        CHECK(clamped.x == doctest::Approx(3.0f));
        CHECK(clamped.y == doctest::Approx(4.0f));
    }

    TEST_CASE("clampedToLength leaves the zero vector unchanged rather than dividing by zero") {
        const Vector2 zero{0.0f, 0.0f};
        CHECK(zero.clampedToLength(5.0f) == zero);
    }

    TEST_CASE("fromAngleDegrees matches the project's screen-angle convention") {
        // 0 degrees points up (negative Y); angles increase clockwise.
        const Vector2 up = Vector2::fromAngleDegrees(0.0f);
        CHECK(up.x == doctest::Approx(0.0f).epsilon(kEpsilon));
        CHECK(up.y == doctest::Approx(-1.0f).epsilon(kEpsilon));

        const Vector2 right = Vector2::fromAngleDegrees(90.0f);
        CHECK(right.x == doctest::Approx(1.0f).epsilon(kEpsilon));
        CHECK(right.y == doctest::Approx(0.0f).epsilon(kEpsilon));

        const Vector2 down = Vector2::fromAngleDegrees(180.0f);
        CHECK(down.x == doctest::Approx(0.0f).epsilon(kEpsilon));
        CHECK(down.y == doctest::Approx(1.0f).epsilon(kEpsilon));

        const Vector2 left = Vector2::fromAngleDegrees(270.0f);
        CHECK(left.x == doctest::Approx(-1.0f).epsilon(kEpsilon));
        CHECK(left.y == doctest::Approx(0.0f).epsilon(kEpsilon));
    }

    TEST_CASE("distanceSquared matches the Pythagorean distance") {
        const Vector2 a{0.0f, 0.0f};
        const Vector2 b{3.0f, 4.0f};
        CHECK(astroparty::core::distanceSquared(a, b) == doctest::Approx(25.0f));
    }
}
