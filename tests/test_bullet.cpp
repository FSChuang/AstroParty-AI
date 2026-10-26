#include <doctest/doctest.h>

#include "astroparty/core/SimulationConfig.hpp"
#include "astroparty/core/Vector2.hpp"
#include "astroparty/entities/Bullet.hpp"

using astroparty::core::SimulationConfig;
using astroparty::core::Vector2;
using astroparty::entities::Bullet;

namespace {
constexpr float kFixedDeltaSeconds = 1.0f / 60.0f;
const SimulationConfig kDefaultConfig{};  // Milestone-1-compatible defaults.
}

TEST_SUITE("Bullet") {
    TEST_CASE("a bullet travels in a straight line at the configured speed") {
        Bullet bullet(/*ownerShipId=*/0, Vector2{100.0f, 100.0f}, /*headingDegrees=*/90.0f,
                       kDefaultConfig);  // heading +X

        bullet.update(kFixedDeltaSeconds);

        const float expectedDistance = kDefaultConfig.bulletSpeedUnitsPerSecond * kFixedDeltaSeconds;
        CHECK(bullet.position().x == doctest::Approx(100.0f + expectedDistance));
        CHECK(bullet.position().y == doctest::Approx(100.0f));
    }

    TEST_CASE("a bullet is not expired immediately after creation") {
        Bullet bullet(0, Vector2{0.0f, 0.0f}, 0.0f, kDefaultConfig);
        CHECK_FALSE(bullet.isExpired());
    }

    TEST_CASE("a bullet expires after approximately its configured lifetime") {
        Bullet bullet(0, Vector2{0.0f, 0.0f}, 0.0f, kDefaultConfig);

        // Count how many fixed-size steps it actually takes to expire rather
        // than asserting an exact step count: repeated float subtraction of
        // deltaSeconds can cross zero one step earlier or later than
        // bulletLifetimeSeconds / kFixedDeltaSeconds suggests, depending on
        // which way that division itself rounds.
        int stepsTaken = 0;
        while (!bullet.isExpired()) {
            REQUIRE(stepsTaken < 1000);  // safety bound in case isExpired() were broken
            bullet.update(kFixedDeltaSeconds);
            ++stepsTaken;
        }

        const double expectedSteps = static_cast<double>(kDefaultConfig.bulletLifetimeSeconds) / kFixedDeltaSeconds;
        CHECK(static_cast<double>(stepsTaken) == doctest::Approx(expectedSteps).epsilon(0.02));
    }

    TEST_CASE("a longer configured lifetime keeps a bullet alive longer") {
        SimulationConfig longLifeConfig = kDefaultConfig;
        longLifeConfig.bulletLifetimeSeconds *= 2.0f;

        Bullet shortLived(0, Vector2{0.0f, 0.0f}, 0.0f, kDefaultConfig);
        Bullet longLived(0, Vector2{0.0f, 0.0f}, 0.0f, longLifeConfig);

        int stepsUntilShortLivedExpires = 0;
        while (!shortLived.isExpired()) {
            shortLived.update(kFixedDeltaSeconds);
            ++stepsUntilShortLivedExpires;
        }
        for (int i = 0; i < stepsUntilShortLivedExpires; ++i) {
            longLived.update(kFixedDeltaSeconds);
        }

        CHECK_FALSE(longLived.isExpired());
    }

    TEST_CASE("expireImmediately ends the bullet's life regardless of remaining lifetime") {
        Bullet bullet(0, Vector2{0.0f, 0.0f}, 0.0f, kDefaultConfig);
        CHECK_FALSE(bullet.isExpired());

        bullet.expireImmediately();

        CHECK(bullet.isExpired());
    }

    TEST_CASE("update has no effect once a bullet has expired") {
        Bullet bullet(0, Vector2{0.0f, 0.0f}, 90.0f, kDefaultConfig);
        bullet.expireImmediately();
        const Vector2 positionAfterExpiry = bullet.position();

        bullet.update(kFixedDeltaSeconds);

        CHECK(bullet.position() == positionAfterExpiry);
    }

    TEST_CASE("ownerShipId is preserved for self-hit exclusion") {
        Bullet bullet(/*ownerShipId=*/7, Vector2{0.0f, 0.0f}, 0.0f, kDefaultConfig);
        CHECK(bullet.ownerShipId() == 7);
    }
}
