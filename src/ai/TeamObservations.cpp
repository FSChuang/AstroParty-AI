#include "astroparty/ai/TeamObservations.hpp"

#include <limits>

namespace astroparty::ai {

using core::Vector2;
using entities::Resource;
using entities::ResourceState;
using entities::Ship;

std::optional<Vector2> homeRegionPosition(const core::World& world, int teamId) noexcept {
    for (const core::HomeRegion& home : world.map().homeRegions) {
        if (home.teamId == teamId) {
            return home.position;
        }
    }
    return std::nullopt;
}

bool isShipCarryingAnyResource(const core::World& world, int shipId) noexcept {
    for (const Resource& resource : world.resources()) {
        if (resource.state() == ResourceState::Carried && resource.carrierShipId() == shipId) {
            return true;
        }
    }
    return false;
}

std::optional<Vector2> nearestAvailableResourcePosition(const core::World& world, Vector2 from) noexcept {
    std::optional<Vector2> nearestPosition;
    float nearestDistanceSquared = std::numeric_limits<float>::max();

    for (const Resource& resource : world.resources()) {
        if (resource.state() != ResourceState::AtSpawn) {
            continue;
        }
        const float distanceSquared = core::distanceSquared(from, resource.spawnPosition());
        if (distanceSquared < nearestDistanceSquared) {
            nearestDistanceSquared = distanceSquared;
            nearestPosition = resource.spawnPosition();
        }
    }
    return nearestPosition;
}

std::optional<int> nearestAliveEnemyShipId(const core::World& world, int ownTeamId, Vector2 from) noexcept {
    std::optional<int> nearestShipId;
    float nearestDistanceSquared = std::numeric_limits<float>::max();

    for (const Ship& ship : world.ships()) {
        if (!ship.isAlive() || world.teamIdOfShip(ship.id()) == ownTeamId) {
            continue;
        }
        const float distanceSquared = core::distanceSquared(from, ship.position());
        if (distanceSquared < nearestDistanceSquared) {
            nearestDistanceSquared = distanceSquared;
            nearestShipId = ship.id();
        }
    }
    return nearestShipId;
}

std::optional<int> mostImminentEnemyCarrierShipId(const core::World& world, int ownTeamId) noexcept {
    std::optional<int> bestShipId;
    float bestDistanceToHomeSquared = std::numeric_limits<float>::max();

    for (const Resource& resource : world.resources()) {
        if (resource.state() != ResourceState::Carried) {
            continue;
        }
        const int carrierShipId = *resource.carrierShipId();
        const int carrierTeamId = world.teamIdOfShip(carrierShipId);
        if (carrierTeamId == ownTeamId) {
            continue;  // Not an enemy.
        }
        const std::optional<Vector2> carrierHome = homeRegionPosition(world, carrierTeamId);
        if (!carrierHome) {
            continue;
        }
        const Ship& carrier = world.ships()[static_cast<std::size_t>(carrierShipId)];
        const float distanceToHomeSquared = core::distanceSquared(carrier.position(), *carrierHome);
        if (distanceToHomeSquared < bestDistanceToHomeSquared) {
            bestDistanceToHomeSquared = distanceToHomeSquared;
            bestShipId = carrierShipId;
        }
    }
    return bestShipId;
}

std::optional<int> friendlyCarrierShipId(const core::World& world, int ownTeamId) noexcept {
    for (const Resource& resource : world.resources()) {
        if (resource.state() != ResourceState::Carried) {
            continue;
        }
        const int carrierShipId = *resource.carrierShipId();
        if (world.teamIdOfShip(carrierShipId) == ownTeamId) {
            return carrierShipId;
        }
    }
    return std::nullopt;
}

}  // namespace astroparty::ai
