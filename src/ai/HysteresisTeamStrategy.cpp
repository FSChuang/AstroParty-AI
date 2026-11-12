#include "astroparty/ai/HysteresisTeamStrategy.hpp"

#include <limits>
#include <optional>

#include "astroparty/ai/TeamObservations.hpp"

namespace astroparty::ai {

using core::Vector2;
using entities::Ship;

namespace {
// Entry thresholds matching UtilityTeamStrategy's own kInterceptRadius/
// kEscortThreatRadius exactly, so both strategies start a given
// Interceptor/Escort response under identical conditions -- only how long
// it is HELD differs. Duplicated here (rather than extracted into a shared
// header) specifically so UtilityTeamStrategy's file is never touched by
// this milestone's work, per "do not modify this baseline silently."
constexpr float kInterceptRadius = 400.0f;
constexpr float kEscortThreatRadius = 250.0f;

// Exit thresholds are deliberately WIDER than the entry thresholds above --
// this gap is the hysteresis band. A ship already committed to
// Interceptor/Escort does not drop that role the instant the enemy dips
// back under the entry radius; it only drops once the enemy clears this
// larger exit radius (or stops being a threat at all), which is what
// prevents a threat hovering near the entry boundary from flipping a
// ship's role every tick.
constexpr float kInterceptExitMultiplier = 1.5f;
constexpr float kEscortExitMultiplier = 1.5f;

// Minimum ticks a new commitment is held before its exit condition is even
// consulted -- 1.5 seconds at the default 60 Hz tick rate. This is the
// other half of the hysteresis: even if the enemy immediately clears the
// (wider) exit radius one tick after the commitment starts, the ship keeps
// its role for at least this long rather than abandoning it instantly.
constexpr int kMinCommitmentTicks = 90;

std::optional<std::size_t> nearestUnassignedShipIndex(const core::World& world, const std::vector<int>& teamShipIds,
                                                        const std::vector<bool>& assigned, Vector2 point) {
    std::optional<std::size_t> bestIndex;
    float bestDistanceSquared = std::numeric_limits<float>::max();
    for (std::size_t i = 0; i < teamShipIds.size(); ++i) {
        if (assigned[i]) continue;
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

std::vector<ShipGoal> HysteresisTeamStrategy::assignGoals(const core::World& world, int teamId,
                                                           const std::vector<int>& teamShipIds) {
    const int tick = world.elapsedTicks();
    CachedTeamResult& cached = cache_[teamId == 1 ? 1 : 0];
    if (cached.teamId == teamId && cached.elapsedTicks == tick) {
        // Every ai::TeamShipController on this team calls assignGoals()
        // independently with identical inputs (see this class's own doc
        // comment) -- without this cache, a stateful strategy like this one
        // would advance its commitment timers once per SHIP instead of
        // once per actual simulation tick.
        return cached.goals;
    }

    cached.goals = computeGoals(world, teamId, teamShipIds);
    cached.teamId = teamId;
    cached.elapsedTicks = tick;
    return cached.goals;
}

std::vector<ShipGoal> HysteresisTeamStrategy::computeGoals(const core::World& world, int teamId,
                                                            const std::vector<int>& teamShipIds) {
    std::vector<ShipGoal> goals(teamShipIds.size());
    std::vector<bool> assigned(teamShipIds.size(), false);
    const Vector2 homePosition = homeRegionPosition(world, teamId).value_or(Vector2{});

    // Priority 0: dead ships get the same harmless placeholder every
    // strategy in this project uses, and drop any commitment they held.
    for (std::size_t i = 0; i < teamShipIds.size(); ++i) {
        const int shipId = teamShipIds[i];
        if (!world.ships()[static_cast<std::size_t>(shipId)].isAlive()) {
            goals[i] = ShipGoal{ShipRole::Defender, homePosition, std::nullopt};
            assigned[i] = true;
            commitmentsByShipId_.erase(shipId);
        }
    }

    // Priority 1: carrying always overrides -- delivering is never
    // something to stay committed away from.
    for (std::size_t i = 0; i < teamShipIds.size(); ++i) {
        if (assigned[i]) continue;
        const int shipId = teamShipIds[i];
        if (isShipCarryingAnyResource(world, shipId)) {
            goals[i] = ShipGoal{ShipRole::Collector, homePosition, std::nullopt};
            assigned[i] = true;
            commitmentsByShipId_.erase(shipId);
        }
    }

    // Priority 2: honor existing commitments that are still valid.
    for (std::size_t i = 0; i < teamShipIds.size(); ++i) {
        if (assigned[i]) continue;
        const int shipId = teamShipIds[i];
        const auto it = commitmentsByShipId_.find(shipId);
        if (it == commitmentsByShipId_.end()) continue;

        Commitment& commitment = it->second;
        const auto targetIndex = static_cast<std::size_t>(commitment.targetShipId);
        if (!world.ships()[targetIndex].isAlive()) {
            commitmentsByShipId_.erase(it);
            continue;
        }
        const Vector2 targetPosition = world.ships()[targetIndex].position();

        bool stillAThreat = false;
        float exitRadiusSquared = 0.0f;
        Vector2 exitReferencePoint = homePosition;
        if (commitment.role == ShipRole::Interceptor) {
            stillAThreat = isShipCarryingAnyResource(world, commitment.targetShipId);
            exitRadiusSquared = (kInterceptRadius * kInterceptExitMultiplier) * (kInterceptRadius * kInterceptExitMultiplier);
            exitReferencePoint = homePosition;
        } else {
            const auto ownCarrier = friendlyCarrierShipId(world, teamId);
            stillAThreat = ownCarrier.has_value();
            exitRadiusSquared =
                (kEscortThreatRadius * kEscortExitMultiplier) * (kEscortThreatRadius * kEscortExitMultiplier);
            exitReferencePoint = ownCarrier ? world.ships()[static_cast<std::size_t>(*ownCarrier)].position() : homePosition;
        }
        const bool withinExitBand = core::distanceSquared(targetPosition, exitReferencePoint) <= exitRadiusSquared;
        const bool keepCommitment = stillAThreat && (commitment.minTicksRemaining > 0 || withinExitBand);

        if (!keepCommitment) {
            commitmentsByShipId_.erase(it);
            continue;
        }
        if (commitment.minTicksRemaining > 0) {
            --commitment.minTicksRemaining;
        }
        goals[i] = ShipGoal{commitment.role, targetPosition, commitment.targetShipId};
        assigned[i] = true;
    }

    // Priority 3: start a new Interceptor commitment, unless a ship is
    // already committed against this exact threat.
    if (const auto enemyCarrierId = mostImminentEnemyCarrierShipId(world, teamId)) {
        const Vector2 carrierPosition = world.ships()[static_cast<std::size_t>(*enemyCarrierId)].position();
        const bool withinEntryRadius =
            core::distanceSquared(carrierPosition, homePosition) <= kInterceptRadius * kInterceptRadius;
        bool alreadyHandled = false;
        for (std::size_t i = 0; i < teamShipIds.size(); ++i) {
            if (assigned[i] && goals[i].role == ShipRole::Interceptor && goals[i].targetShipId == enemyCarrierId) {
                alreadyHandled = true;
                break;
            }
        }
        if (withinEntryRadius && !alreadyHandled) {
            if (const auto index = nearestUnassignedShipIndex(world, teamShipIds, assigned, carrierPosition)) {
                commitmentsByShipId_[teamShipIds[*index]] =
                    Commitment{ShipRole::Interceptor, *enemyCarrierId, kMinCommitmentTicks};
                goals[*index] = ShipGoal{ShipRole::Interceptor, carrierPosition, enemyCarrierId};
                assigned[*index] = true;
            }
        }
    }

    // Priority 4: start a new Escort commitment, unless already handled.
    if (const auto ownCarrierId = friendlyCarrierShipId(world, teamId)) {
        const Vector2 carrierPosition = world.ships()[static_cast<std::size_t>(*ownCarrierId)].position();
        if (const auto threatId = nearestAliveEnemyShipId(world, teamId, carrierPosition)) {
            const Vector2 threatPosition = world.ships()[static_cast<std::size_t>(*threatId)].position();
            const bool withinEntryRadius =
                core::distanceSquared(threatPosition, carrierPosition) <= kEscortThreatRadius * kEscortThreatRadius;
            bool alreadyHandled = false;
            for (std::size_t i = 0; i < teamShipIds.size(); ++i) {
                if (assigned[i] && goals[i].role == ShipRole::Escort && goals[i].targetShipId == threatId) {
                    alreadyHandled = true;
                    break;
                }
            }
            if (withinEntryRadius && !alreadyHandled) {
                if (const auto index = nearestUnassignedShipIndex(world, teamShipIds, assigned, carrierPosition)) {
                    commitmentsByShipId_[teamShipIds[*index]] =
                        Commitment{ShipRole::Escort, *threatId, kMinCommitmentTicks};
                    goals[*index] = ShipGoal{ShipRole::Escort, threatPosition, threatId};
                    assigned[*index] = true;
                }
            }
        }
    }

    // Priority 5: everyone remaining gathers the nearest available resource.
    for (std::size_t i = 0; i < teamShipIds.size(); ++i) {
        if (assigned[i]) continue;
        const Ship& ship = world.ships()[static_cast<std::size_t>(teamShipIds[i])];
        if (const auto resourcePosition = nearestAvailableResourcePosition(world, ship.position())) {
            goals[i] = ShipGoal{ShipRole::Collector, *resourcePosition, std::nullopt};
            assigned[i] = true;
        }
    }

    // Priority 6: anyone left over patrols home.
    for (std::size_t i = 0; i < teamShipIds.size(); ++i) {
        if (!assigned[i]) {
            goals[i] = ShipGoal{ShipRole::Defender, homePosition, std::nullopt};
        }
    }

    return goals;
}

}  // namespace astroparty::ai
