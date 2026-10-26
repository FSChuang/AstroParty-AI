#include "astroparty/ai/BaselineTeamStrategy.hpp"

#include "astroparty/ai/TeamObservations.hpp"

namespace astroparty::ai {

using core::Vector2;
using entities::Ship;

namespace {
// How close an enemy must come to the Defender's home region before the
// Defender breaks off patrolling to engage it. Chosen by eye relative to
// the team_arena map scale; not derived from any other constant.
constexpr float kDefenseEngagementRadius = 220.0f;
}  // namespace

std::vector<ShipGoal> BaselineTeamStrategy::assignGoals(const core::World& world, int teamId,
                                                         const std::vector<int>& teamShipIds) {
    std::vector<ShipGoal> goals;
    goals.reserve(teamShipIds.size());

    const Vector2 homePosition = homeRegionPosition(world, teamId).value_or(Vector2{});

    for (std::size_t index = 0; index < teamShipIds.size(); ++index) {
        const int shipId = teamShipIds[index];
        ShipGoal goal;

        // A ship already carrying a resource always heads home to deliver,
        // overriding its normal fixed role.
        if (isShipCarryingAnyResource(world, shipId)) {
            goal.role = ShipRole::Collector;
            goal.targetPosition = homePosition;
            goals.push_back(goal);
            continue;
        }

        const bool isDefender = (index + 1 == teamShipIds.size());  // Fixed: only the last ship.
        if (!isDefender) {
            goal.role = ShipRole::Collector;
            const Ship& ship = world.ships()[static_cast<std::size_t>(shipId)];
            goal.targetPosition = nearestAvailableResourcePosition(world, ship.position()).value_or(homePosition);
        } else {
            goal.role = ShipRole::Defender;
            const auto nearbyEnemyId = nearestAliveEnemyShipId(world, teamId, homePosition);
            if (nearbyEnemyId &&
                core::distanceSquared(homePosition, world.ships()[static_cast<std::size_t>(*nearbyEnemyId)].position()) <=
                    kDefenseEngagementRadius * kDefenseEngagementRadius) {
                goal.targetShipId = nearbyEnemyId;
                goal.targetPosition = world.ships()[static_cast<std::size_t>(*nearbyEnemyId)].position();
            } else {
                goal.targetPosition = homePosition;  // Patrol.
            }
        }
        goals.push_back(goal);
    }

    return goals;
}

}  // namespace astroparty::ai
