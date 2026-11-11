#include "astroparty/ai/UtilityTeamStrategy.hpp"

#include <algorithm>
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
constexpr float kNearbyThreatRadius = 200.0f;
constexpr float kSelfDefenseRadius = 100.0f;
constexpr float kNonCarrierEscortRadius = 125.0f;

bool isNearby(const core::World& world, const Ship& ship, int enemyId) {
    return core::distanceSquared(ship.position(), world.ships()[enemyId].position()) <=
           kNearbyThreatRadius * kNearbyThreatRadius;
}

ShipGoal collectorGoal(const core::World& world, int teamId, const Ship& ship, Vector2 destination,
                       bool carrying = false) {
    const auto enemy = nearestAliveEnemyShipId(world, teamId, ship.position());
    if (enemy && !carrying &&
        core::distanceSquared(ship.position(), world.ships()[*enemy].position()) <= kSelfDefenseRadius * kSelfDefenseRadius) {
        return ShipGoal{ShipRole::SelfDefense, world.ships()[*enemy].position(), enemy};
    }
    return ShipGoal{ShipRole::Collector, destination,
                    enemy && isNearby(world, ship, *enemy) ? enemy : std::nullopt};
}

std::optional<ShipGoal> escortGoal(const core::World& world, int teamId, const std::vector<int>& shipIds,
                                   float carrierRadius, std::optional<int> escortId) {
    std::optional<ShipGoal> best;
    bool bestIsCarrier = false;
    float bestDistance = std::numeric_limits<float>::max();
    for (const int allyId : shipIds) {
        const auto& ally = world.ships()[allyId];
        if (!ally.isAlive() || allyId == escortId) {
            continue;
        }
        const bool carrying = isShipCarryingAnyResource(world, allyId);
        // A reserve ship can protect any ally. If we're short a reserve,
        // pull a ship off its current job only to protect a carrier.
        if (!carrying && (!escortId || bestIsCarrier)) {
            continue;
        }
        const auto enemy = nearestAliveEnemyShipId(world, teamId, ally.position());
        if (!enemy) {
            continue;
        }
        const auto enemyPosition = world.ships()[*enemy].position();
        const float distance = core::distanceSquared(ally.position(), enemyPosition);
        const float radius = carrying ? carrierRadius : kNonCarrierEscortRadius;
        if (distance <= radius * radius && ((carrying && !bestIsCarrier) || distance < bestDistance)) {
            best = ShipGoal{ShipRole::Escort, enemyPosition, enemy, allyId};
            bestDistance = distance;
            bestIsCarrier = carrying;
        }
    }
    return best;
}

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

ShipGoal UtilityTeamStrategy::supportGoal(const core::World& world, int teamId, const Ship& ship,
                                          Vector2 homePosition, std::optional<int> enemyCarrierId) const {
    const auto nearby = nearestAliveEnemyShipId(world, teamId, ship.position());
    const auto target = nearby && (!enemyCarrierId || isNearby(world, ship, *nearby))
                            ? nearby : enemyCarrierId;
    return target ? ShipGoal{ShipRole::Interceptor, world.ships()[*target].position(), target}
                  : ShipGoal{ShipRole::Defender, homePosition};
}

std::vector<ShipGoal> UtilityTeamStrategy::assignGoals(const core::World& world, int teamId,
                                                        const std::vector<int>& teamShipIds) {
    std::vector<ShipGoal> goals(teamShipIds.size());
    std::vector<bool> assigned(teamShipIds.size(), false);
    const Vector2 homePosition = homeRegionPosition(world, teamId).value_or(Vector2{});
    std::size_t collectorCount = 0;
    std::size_t livingCount = teamShipIds.size();

    // Dead ships are excluded from assignment entirely; they get a harmless
    // placeholder goal (Ship::update() is a no-op for a dead ship anyway).
    for (std::size_t i = 0; i < teamShipIds.size(); ++i) {
        if (!world.ships()[static_cast<std::size_t>(teamShipIds[i])].isAlive()) {
            goals[i] = ShipGoal{ShipRole::Defender, homePosition, std::nullopt};
            assigned[i] = true;
            --livingCount;
        }
    }

    // Priority 1: a ship already carrying a resource always heads home.
    for (std::size_t i = 0; i < teamShipIds.size(); ++i) {
        if (assigned[i]) {
            continue;
        }
        if (isShipCarryingAnyResource(world, teamShipIds[i])) {
            goals[i] = collectorGoal(world, teamId, world.ships()[teamShipIds[i]], homePosition, true);
            assigned[i] = true;
            ++collectorCount;
        }
    }

    const auto enemyCarrierId = mostImminentEnemyCarrierShipId(world, teamId);
    const auto availableCore = nearestAvailableResourcePosition(world, homePosition);

    // Reserve one offensive slot when a full team can send two ships to
    // collect or deliver. Prefer the same ship until it is unavailable, so
    // small changes in distance do not constantly swap the attack role.
    if (livingCount == 3 && collectorCount < 3 && (collectorCount == 2 || availableCore)) {
        for (std::size_t i = teamShipIds.size(); i-- > 0;) {
            if (assigned[i]) {
                continue;
            }
            const Ship& ship = world.ships()[teamShipIds[i]];
            if (const auto escort = escortGoal(world, teamId, teamShipIds, escortThreatRadius_, ship.id())) {
                goals[i] = *escort;
            } else {
                goals[i] = supportGoal(world, teamId, ship, homePosition, enemyCarrierId);
            }
            assigned[i] = true;
            std::optional<Vector2> reservedCore;
            for (std::size_t j = 0; j < teamShipIds.size(); ++j) {
                if (!assigned[j]) {
                    const auto& collector = world.ships()[teamShipIds[j]];
                    const auto nearest = nearestAvailableResourcePosition(world, collector.position(), reservedCore);
                    const auto core = nearest.value_or(availableCore.value_or(homePosition));
                    goals[j] = collectorGoal(world, teamId, collector, core);
                    if (goals[j].role == ShipRole::Collector) {
                        reservedCore = core;
                    }
                }
            }
            return goals;
        }
    }

    // With fewer collectors available, respond to immediate threats.
    if (const auto escort = escortGoal(world, teamId, teamShipIds, escortThreatRadius_, std::nullopt)) {
        const auto& ally = world.ships()[*escort->protectedShipId];
        if (const auto index = nearestUnassignedShipIndex(world, teamShipIds, assigned, ally.position())) {
            goals[*index] = *escort;
            assigned[*index] = true;
        }
    }
    if (enemyCarrierId) {
        const Ship& carrier = world.ships()[static_cast<std::size_t>(*enemyCarrierId)];
        const bool alreadyEngaged = std::any_of(goals.begin(), goals.end(), [&](const ShipGoal& goal) {
            return goal.role == ShipRole::Escort && goal.targetShipId == enemyCarrierId;
        });
        if (!alreadyEngaged && core::distanceSquared(carrier.position(), homePosition) <= kInterceptRadius * kInterceptRadius) {
            if (const auto index = nearestUnassignedShipIndex(world, teamShipIds, assigned, carrier.position())) {
                goals[*index] = ShipGoal{ShipRole::Interceptor, carrier.position(), enemyCarrierId};
                assigned[*index] = true;
            }
        }
    }

    // Send the remaining ships after the nearest available resource.
    for (std::size_t i = 0; i < teamShipIds.size(); ++i) {
        if (assigned[i]) {
            continue;
        }
        const Ship& ship = world.ships()[static_cast<std::size_t>(teamShipIds[i])];
        if (const auto resourcePosition = nearestAvailableResourcePosition(world, ship.position())) {
            goals[i] = collectorGoal(world, teamId, ship, *resourcePosition);
            assigned[i] = true;
        }
    }

    // Anyone left over (e.g. no resources are currently
    // available to chase) patrols home.
    for (std::size_t i = 0; i < teamShipIds.size(); ++i) {
        if (!assigned[i]) {
            const auto& ship = world.ships()[teamShipIds[i]];
            const auto defense = collectorGoal(world, teamId, ship, homePosition);
            goals[i] = defense.role == ShipRole::SelfDefense ? defense : ShipGoal{ShipRole::Defender, homePosition};
        }
    }

    return goals;
}

}  // namespace astroparty::ai
