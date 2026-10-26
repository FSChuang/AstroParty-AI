#include "astroparty/ai/UtilityTeamStrategy.hpp"

#include <limits>
#include <optional>

#include "astroparty/ai/TeamObservations.hpp"

namespace astroparty::ai {

using core::Vector2;
using entities::Ship;

namespace {
// Chosen by eye relative to the team_arena map scale (1280x720); neither is
// derived from any other project constant.
constexpr float kInterceptRadius = 400.0f;      // How close an enemy carrier must get to our home to intercept.
constexpr float kEscortThreatRadius = 250.0f;   // How close an enemy must get to our carrier to send an escort.

// Returns the index (into teamShipIds/assigned) of the not-yet-assigned
// ship closest to `point`, or std::nullopt if every ship is already
// assigned.
std::optional<std::size_t> nearestUnassignedShipIndex(const core::World& world, const std::vector<int>& teamShipIds,
                                                        const std::vector<bool>& assigned, Vector2 point) {
    std::optional<std::size_t> bestIndex;
    float bestDistanceSquared = std::numeric_limits<float>::max();
    for (std::size_t i = 0; i < teamShipIds.size(); ++i) {
        if (assigned[i]) {
            continue;
        }
        const Ship& ship = world.ships()[static_cast<std::size_t>(teamShipIds[i])];
        const float distanceSquared = core::distanceSquared(ship.position(), point);
        if (distanceSquared < bestDistanceSquared) {
            bestDistanceSquared = distanceSquared;
            bestIndex = i;
        }
    }
    return bestIndex;
}
}  // namespace

std::vector<ShipGoal> UtilityTeamStrategy::assignGoals(const core::World& world, int teamId,
                                                        const std::vector<int>& teamShipIds) {
    std::vector<ShipGoal> goals(teamShipIds.size());
    std::vector<bool> assigned(teamShipIds.size(), false);
    const Vector2 homePosition = homeRegionPosition(world, teamId).value_or(Vector2{});

    // Dead ships are excluded from assignment entirely; they get a harmless
    // placeholder goal (Ship::update() is a no-op for a dead ship anyway).
    for (std::size_t i = 0; i < teamShipIds.size(); ++i) {
        if (!world.ships()[static_cast<std::size_t>(teamShipIds[i])].isAlive()) {
            goals[i] = ShipGoal{ShipRole::Defender, homePosition, std::nullopt};
            assigned[i] = true;
        }
    }

    // Priority 1: a ship already carrying a resource always heads home.
    for (std::size_t i = 0; i < teamShipIds.size(); ++i) {
        if (assigned[i]) {
            continue;
        }
        if (isShipCarryingAnyResource(world, teamShipIds[i])) {
            goals[i] = ShipGoal{ShipRole::Collector, homePosition, std::nullopt};
            assigned[i] = true;
        }
    }

    // Priority 2: intercept an enemy carrier closing in on our home.
    if (const auto enemyCarrierId = mostImminentEnemyCarrierShipId(world, teamId)) {
        const Ship& carrier = world.ships()[static_cast<std::size_t>(*enemyCarrierId)];
        if (core::distanceSquared(carrier.position(), homePosition) <= kInterceptRadius * kInterceptRadius) {
            if (const auto index = nearestUnassignedShipIndex(world, teamShipIds, assigned, carrier.position())) {
                goals[*index] = ShipGoal{ShipRole::Interceptor, carrier.position(), enemyCarrierId};
                assigned[*index] = true;
            }
        }
    }

    // Priority 3: escort our own carrier if an enemy is closing in on it.
    if (const auto ownCarrierId = friendlyCarrierShipId(world, teamId)) {
        const Ship& carrier = world.ships()[static_cast<std::size_t>(*ownCarrierId)];
        if (const auto threatId = nearestAliveEnemyShipId(world, teamId, carrier.position())) {
            const Ship& threat = world.ships()[static_cast<std::size_t>(*threatId)];
            if (core::distanceSquared(threat.position(), carrier.position()) <= kEscortThreatRadius * kEscortThreatRadius) {
                if (const auto index = nearestUnassignedShipIndex(world, teamShipIds, assigned, carrier.position())) {
                    goals[*index] = ShipGoal{ShipRole::Escort, threat.position(), threatId};
                    assigned[*index] = true;
                }
            }
        }
    }

    // Priority 4: send every remaining available ship after the nearest
    // currently-available resource.
    for (std::size_t i = 0; i < teamShipIds.size(); ++i) {
        if (assigned[i]) {
            continue;
        }
        const Ship& ship = world.ships()[static_cast<std::size_t>(teamShipIds[i])];
        if (const auto resourcePosition = nearestAvailableResourcePosition(world, ship.position())) {
            goals[i] = ShipGoal{ShipRole::Collector, *resourcePosition, std::nullopt};
            assigned[i] = true;
        }
    }

    // Priority 5: anyone left over (e.g. no resources are currently
    // available to chase) patrols home.
    for (std::size_t i = 0; i < teamShipIds.size(); ++i) {
        if (!assigned[i]) {
            goals[i] = ShipGoal{ShipRole::Defender, homePosition, std::nullopt};
        }
    }

    return goals;
}

}  // namespace astroparty::ai
