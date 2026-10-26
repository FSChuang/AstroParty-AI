#include <doctest/doctest.h>

#include <vector>

#include "astroparty/core/Collisions.hpp"
#include "astroparty/core/SimulationConfig.hpp"
#include "astroparty/core/Vector2.hpp"
#include "astroparty/entities/Bullet.hpp"
#include "astroparty/entities/Ship.hpp"

using astroparty::core::resolveShipBulletCollisions;
using astroparty::core::SimulationConfig;
using astroparty::core::Vector2;
using astroparty::entities::Bullet;
using astroparty::entities::Ship;

namespace {
const SimulationConfig kDefaultConfig{};
const float kCollisionRadius = kDefaultConfig.shipCollisionRadius;
}

TEST_SUITE("resolveShipBulletCollisions") {
    TEST_CASE("an enemy bullet overlapping a ship eliminates it and expires the bullet") {
        std::vector<Ship> ships;
        ships.emplace_back(0, Vector2{100.0f, 100.0f}, 0.0f);
        ships.emplace_back(1, Vector2{200.0f, 100.0f}, 0.0f);

        std::vector<Bullet> bullets;
        bullets.emplace_back(/*ownerShipId=*/0, /*spawnPosition=*/Vector2{200.0f, 100.0f}, 0.0f, kDefaultConfig);

        resolveShipBulletCollisions(ships, bullets, kCollisionRadius);

        CHECK_FALSE(ships[1].isAlive());
        CHECK(ships[0].isAlive());
        CHECK(bullets[0].isExpired());
    }

    TEST_CASE("a ship's own bullet never eliminates it") {
        std::vector<Ship> ships;
        ships.emplace_back(0, Vector2{100.0f, 100.0f}, 0.0f);

        std::vector<Bullet> bullets;
        bullets.emplace_back(/*ownerShipId=*/0, /*spawnPosition=*/Vector2{100.0f, 100.0f}, 0.0f, kDefaultConfig);

        resolveShipBulletCollisions(ships, bullets, kCollisionRadius);

        CHECK(ships[0].isAlive());
        CHECK_FALSE(bullets[0].isExpired());
    }

    TEST_CASE("a bullet far from every ship eliminates nothing") {
        std::vector<Ship> ships;
        ships.emplace_back(0, Vector2{0.0f, 0.0f}, 0.0f);

        std::vector<Bullet> bullets;
        bullets.emplace_back(1, Vector2{10000.0f, 10000.0f}, 0.0f, kDefaultConfig);

        resolveShipBulletCollisions(ships, bullets, kCollisionRadius);

        CHECK(ships[0].isAlive());
        CHECK_FALSE(bullets[0].isExpired());
    }

    TEST_CASE("an already-eliminated ship is skipped") {
        std::vector<Ship> ships;
        ships.emplace_back(0, Vector2{100.0f, 100.0f}, 0.0f);
        ships[0].eliminate();

        std::vector<Bullet> bullets;
        bullets.emplace_back(1, Vector2{100.0f, 100.0f}, 0.0f, kDefaultConfig);

        resolveShipBulletCollisions(ships, bullets, kCollisionRadius);

        // The bullet should not be consumed by a ship that is already out of the match.
        CHECK_FALSE(bullets[0].isExpired());
    }

    TEST_CASE("a smaller configured collision radius requires a closer hit") {
        std::vector<Ship> ships;
        ships.emplace_back(0, Vector2{100.0f, 100.0f}, 0.0f);

        std::vector<Bullet> bullets;
        // 10 units away: within the default radius (18) but outside a radius of 5.
        bullets.emplace_back(1, Vector2{110.0f, 100.0f}, 0.0f, kDefaultConfig);

        resolveShipBulletCollisions(ships, bullets, /*shipCollisionRadius=*/5.0f);

        CHECK(ships[0].isAlive());
        CHECK_FALSE(bullets[0].isExpired());
    }
}
