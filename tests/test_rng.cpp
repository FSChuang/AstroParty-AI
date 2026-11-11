#include <doctest/doctest.h>

#include "astroparty/procgen/Rng.hpp"

using astroparty::procgen::Rng;

TEST_SUITE("procgen::Rng") {
    TEST_CASE("the same seed produces the same sequence of draws") {
        Rng first(42);
        Rng second(42);

        for (int i = 0; i < 20; ++i) {
            CHECK(first.uniformFloat(0.0f, 100.0f) == second.uniformFloat(0.0f, 100.0f));
        }
    }

    TEST_CASE("different seeds produce different sequences") {
        Rng first(1);
        Rng second(2);

        bool anyDifference = false;
        for (int i = 0; i < 20; ++i) {
            if (first.uniformFloat(0.0f, 1000.0f) != second.uniformFloat(0.0f, 1000.0f)) {
                anyDifference = true;
            }
        }
        CHECK(anyDifference);
    }

    TEST_CASE("uniformFloat stays within the requested bounds") {
        Rng rng(7);
        for (int i = 0; i < 200; ++i) {
            const float value = rng.uniformFloat(10.0f, 20.0f);
            CHECK(value >= 10.0f);
            CHECK(value < 20.0f);
        }
    }

    TEST_CASE("uniformInt stays within the inclusive requested bounds") {
        Rng rng(7);
        for (int i = 0; i < 200; ++i) {
            const int value = rng.uniformInt(3, 5);
            CHECK(value >= 3);
            CHECK(value <= 5);
        }
    }

    TEST_CASE("uniformInRectangle stays within the requested corners") {
        Rng rng(7);
        const astroparty::core::Vector2 minCorner{10.0f, 20.0f};
        const astroparty::core::Vector2 maxCorner{50.0f, 80.0f};
        for (int i = 0; i < 100; ++i) {
            const astroparty::core::Vector2 point = rng.uniformInRectangle(minCorner, maxCorner);
            CHECK(point.x >= minCorner.x);
            CHECK(point.x <= maxCorner.x);
            CHECK(point.y >= minCorner.y);
            CHECK(point.y <= maxCorner.y);
        }
    }
}
