#include "astroparty/core/World.hpp"

#include "astroparty/core/Collisions.hpp"
#include "astroparty/physics/PhysicsConstants.hpp"

namespace astroparty::core {

namespace {

// Fixed two-ship starting layout for Milestone 1. Ships spawn in opposite
// corners, angled toward the arena's center rather than upstream's uniform
// 180-degree default (astro-party-workspace/audit/03_code_architecture.md,
// section 10), so a freshly started or restarted match reads clearly instead
// of both ships immediately flying toward a screen edge.
struct ShipSpawn {
    Vector2 position;
    float headingDegrees;
};

constexpr ShipSpawn kShipSpawns[] = {
    {Vector2{120.0f, 120.0f}, 135.0f},
    {Vector2{1160.0f, 600.0f}, 315.0f},
};

}  // namespace

World::World() : arenaSize_(physics::kArenaSize) { spawnInitialShips(); }

void World::spawnInitialShips() {
    ships_.clear();
    int nextShipId = 0;
    for (const ShipSpawn& spawn : kShipSpawns) {
        ships_.emplace_back(nextShipId++, spawn.position, spawn.headingDegrees);
    }
}

void World::step(const std::vector<ShipAction>& actions, float deltaSeconds) {
    if (matchState_.hasEnded()) {
        return;
    }

    updateShips(actions, deltaSeconds);
    updateBullets(deltaSeconds);
    resolveCollisions();
    removeExpiredBullets();
    matchState_.evaluate(ships_);
}

void World::reset() {
    bullets_.clear();
    spawnInitialShips();
    matchState_.reset();
}

void World::updateShips(const std::vector<ShipAction>& actions, float deltaSeconds) {
    static constexpr ShipAction kNoInput{};
    for (std::size_t i = 0; i < ships_.size(); ++i) {
        const ShipAction& action = (i < actions.size()) ? actions[i] : kNoInput;
        if (const auto spawnRequest = ships_[i].update(action, deltaSeconds, arenaSize_)) {
            bullets_.emplace_back(ships_[i].id(), spawnRequest->position, spawnRequest->headingDegrees);
        }
    }
}

void World::updateBullets(float deltaSeconds) {
    for (entities::Bullet& bullet : bullets_) {
        bullet.update(deltaSeconds);
    }
}

void World::resolveCollisions() { resolveShipBulletCollisions(ships_, bullets_); }

void World::removeExpiredBullets() {
    std::erase_if(bullets_, [](const entities::Bullet& bullet) { return bullet.isExpired(); });
}

}  // namespace astroparty::core
