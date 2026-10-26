#include "astroparty/core/World.hpp"

#include <utility>

#include "astroparty/core/Collisions.hpp"

namespace astroparty::core {

World::World() : World(SimulationConfig{}, MapDefinition::defaultTwoShipMap()) {}

World::World(SimulationConfig config, MapDefinition map) : config_(config), map_(std::move(map)) {
    config_.arenaSize = map_.arenaSize;  // The map is the single source of truth for arena bounds.
    spawnInitialShips();
}

void World::spawnInitialShips() {
    ships_.clear();
    int nextShipId = 0;
    for (const ShipSpawnPoint& spawn : map_.shipSpawns) {
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
        if (const auto spawnRequest = ships_[i].update(action, deltaSeconds, config_)) {
            bullets_.emplace_back(ships_[i].id(), spawnRequest->position, spawnRequest->headingDegrees, config_);
        }
    }
}

void World::updateBullets(float deltaSeconds) {
    for (entities::Bullet& bullet : bullets_) {
        bullet.update(deltaSeconds);
    }
}

void World::resolveCollisions() { resolveShipBulletCollisions(ships_, bullets_, config_.shipCollisionRadius); }

void World::removeExpiredBullets() {
    std::erase_if(bullets_, [](const entities::Bullet& bullet) { return bullet.isExpired(); });
}

}  // namespace astroparty::core
