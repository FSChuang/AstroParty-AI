#include "astroparty/core/World.hpp"

#include <stdexcept>
#include <utility>

#include "astroparty/core/Collisions.hpp"
#include "astroparty/physics/Collision.hpp"

namespace astroparty::core {

World::World() : World(SimulationConfig{}, MapDefinition::defaultTwoShipMap()) {}

World::World(SimulationConfig config, MapDefinition map) : config_(config), map_(std::move(map)) {
    config_.arenaSize = map_.arenaSize;  // The map is the single source of truth for arena bounds.
    if (config_.mode == GameMode::TeamResourceBattle) {
        validateTeamResourceBattleRequirements();
    }
    spawnInitialShips();
    spawnInitialResources();
    respawnTicksRemaining_.assign(ships_.size(), 0);
}

void World::validateTeamResourceBattleRequirements() const {
    std::array<int, 2> shipsPerTeam{0, 0};
    for (const ShipSpawnPoint& spawn : map_.shipSpawns) {
        if (spawn.teamId != 0 && spawn.teamId != 1) {
            throw std::invalid_argument(
                "Team Resource Battle requires every ship spawn to belong to team 0 or team 1");
        }
        ++shipsPerTeam[static_cast<std::size_t>(spawn.teamId)];
    }
    if (shipsPerTeam[0] != 3 || shipsPerTeam[1] != 3) {
        throw std::invalid_argument("Team Resource Battle requires exactly three ships per team (3v3)");
    }

    bool team0HasHome = false;
    bool team1HasHome = false;
    for (const HomeRegion& home : map_.homeRegions) {
        if (home.teamId == 0) team0HasHome = true;
        if (home.teamId == 1) team1HasHome = true;
    }
    if (!team0HasHome || !team1HasHome) {
        throw std::invalid_argument("Team Resource Battle requires a home region for both team 0 and team 1");
    }

    if (map_.resourceSpawnPoints.empty()) {
        throw std::invalid_argument("Team Resource Battle requires at least one resource spawn point");
    }
}

void World::spawnInitialShips() {
    ships_.clear();
    int nextShipId = 0;
    for (const ShipSpawnPoint& spawn : map_.shipSpawns) {
        ships_.emplace_back(nextShipId++, spawn.position, spawn.headingDegrees);
    }
}

void World::spawnInitialResources() {
    resources_.clear();
    int nextResourceId = 0;
    for (const Vector2& spawnPoint : map_.resourceSpawnPoints) {
        resources_.emplace_back(nextResourceId++, spawnPoint);
    }
}

void World::step(const std::vector<ShipAction>& actions, float deltaSeconds) {
    const bool matchHasEnded =
        (config_.mode == GameMode::ClassicDeathmatch) ? matchState_.hasEnded() : teamMatchState_.hasEnded();
    if (matchHasEnded) {
        return;
    }

    updateShips(actions, deltaSeconds);
    updateBullets(deltaSeconds);
    resolveCollisions();
    removeExpiredBullets();

    if (config_.mode == GameMode::ClassicDeathmatch) {
        matchState_.evaluate(ships_);
        return;
    }

    dropResourcesCarriedByEliminatedShips();
    updateResourcePickupAndDelivery();
    updateRespawns();
    ++elapsedTicks_;
    teamMatchState_.evaluate(teamScores_, config_.winningScore, elapsedTicks_, config_.maxMatchTicks);
}

void World::reset() {
    bullets_.clear();
    spawnInitialShips();
    spawnInitialResources();
    respawnTicksRemaining_.assign(ships_.size(), 0);
    teamScores_ = {};
    resourcesCollected_ = {};
    shipsDestroyed_ = {};
    elapsedTicks_ = 0;
    matchState_.reset();
    teamMatchState_.reset();
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

void World::resolveCollisions() {
    resolveShipObstacleCollisions(ships_, map_.obstacles, config_.shipCollisionRadius);
    resolveBulletObstacleCollisions(bullets_, map_.obstacles);
    resolveShipBulletCollisions(ships_, bullets_, config_.shipCollisionRadius);
}

void World::removeExpiredBullets() {
    std::erase_if(bullets_, [](const entities::Bullet& bullet) { return bullet.isExpired(); });
}

int World::teamIdOfShip(int shipId) const noexcept {
    const auto index = static_cast<std::size_t>(shipId);
    if (index >= map_.shipSpawns.size()) {
        return -1;
    }
    return map_.shipSpawns[index].teamId;
}

bool World::isCarryingAnyResource(int shipId) const noexcept {
    for (const entities::Resource& resource : resources_) {
        if (resource.state() == entities::ResourceState::Carried && resource.carrierShipId() == shipId) {
            return true;
        }
    }
    return false;
}

void World::dropResourcesCarriedByEliminatedShips() {
    for (entities::Resource& resource : resources_) {
        if (resource.state() != entities::ResourceState::Carried) {
            continue;
        }
        const auto carrierIndex = static_cast<std::size_t>(*resource.carrierShipId());
        if (!ships_[carrierIndex].isAlive()) {
            resource.returnToSpawn();
        }
    }
}

void World::updateResourcePickupAndDelivery() {
    // Pickup: an alive ship not already carrying anything collects any
    // available (AtSpawn) resource within pickup range. A ship carries at
    // most one resource at a time -- the simplest rule that still makes
    // "defend the carrier" and "intercept the carrier" meaningful roles.
    for (entities::Ship& ship : ships_) {
        if (!ship.isAlive() || isCarryingAnyResource(ship.id())) {
            continue;
        }
        for (entities::Resource& resource : resources_) {
            if (resource.state() != entities::ResourceState::AtSpawn) {
                continue;
            }
            if (physics::isPointWithinRadius(ship.position(), resource.spawnPosition(), config_.resourcePickupRadius)) {
                resource.pickUp(ship.id());
                ++resourcesCollected_[static_cast<std::size_t>(teamIdOfShip(ship.id()))];
                break;
            }
        }
    }

    // Delivery: a ship carrying a resource that reaches its OWN team's home
    // region scores a point; the resource immediately becomes available
    // again at its original spawn point.
    for (entities::Resource& resource : resources_) {
        if (resource.state() != entities::ResourceState::Carried) {
            continue;
        }
        const int carrierId = *resource.carrierShipId();
        const entities::Ship& carrier = ships_[static_cast<std::size_t>(carrierId)];
        if (!carrier.isAlive()) {
            continue;  // Handled by dropResourcesCarriedByEliminatedShips.
        }
        const int teamId = teamIdOfShip(carrierId);
        for (const HomeRegion& home : map_.homeRegions) {
            if (home.teamId != teamId) {
                continue;
            }
            if (physics::isPointWithinRadius(carrier.position(), home.position, home.radius)) {
                teamScores_[static_cast<std::size_t>(teamId)] += 1;
                resource.returnToSpawn();
                break;
            }
        }
    }
}

void World::updateRespawns() {
    for (std::size_t i = 0; i < ships_.size(); ++i) {
        if (ships_[i].isAlive()) {
            continue;
        }
        if (respawnTicksRemaining_[i] <= 0) {
            respawnTicksRemaining_[i] = config_.respawnDelayTicks;
            ++shipsDestroyed_[static_cast<std::size_t>(teamIdOfShip(static_cast<int>(i)))];
        }
        --respawnTicksRemaining_[i];
        if (respawnTicksRemaining_[i] <= 0) {
            const ShipSpawnPoint& spawn = map_.shipSpawns[i];
            ships_[i].respawn(spawn.position, spawn.headingDegrees);
        }
    }
}

}  // namespace astroparty::core
