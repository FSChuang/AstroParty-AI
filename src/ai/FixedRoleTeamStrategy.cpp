#include "astroparty/ai/FixedRoleTeamStrategy.hpp"

#include "astroparty/ai/TeamObservations.hpp"

namespace astroparty::ai {

using core::Vector2;
using entities::Ship;

std::vector<ShipGoal> FixedRoleTeamStrategy::assignGoals(const core::World& world, int teamId,
                                                          const std::vector<int>& teamShipIds) {
    std::vector<ShipGoal> goals;
    goals.reserve(teamShipIds.size());

    const Vector2 homePosition = homeRegionPosition(world, teamId).value_or(Vector2{});

    for (std::size_t index = 0; index < teamShipIds.size(); ++index) {
        const int shipId = teamShipIds[index];
        const Ship& ship = world.ships()[static_cast<std::size_t>(shipId)];

        if (!ship.isAlive()) {
            goals.push_back(ShipGoal{ShipRole::Defender, homePosition, std::nullopt});
            continue;
        }
        if (isShipCarryingAnyResource(world, shipId)) {
            goals.push_back(ShipGoal{ShipRole::Collector, homePosition, std::nullopt});
            continue;
        }

        switch (index % 3) {
            case 0: {
                goals.push_back(
                    ShipGoal{ShipRole::Collector, nearestAvailableResourcePosition(world, ship.position())
                                                       .value_or(homePosition),
                             std::nullopt});
                break;
            }
            case 1: {
                goals.push_back(ShipGoal{ShipRole::Defender, homePosition, std::nullopt});
                break;
            }
            default: {
                if (const auto enemyId = nearestAliveEnemyShipId(world, teamId, ship.position())) {
                    goals.push_back(ShipGoal{ShipRole::Interceptor,
                                              world.ships()[static_cast<std::size_t>(*enemyId)].position(), enemyId});
                } else {
                    goals.push_back(ShipGoal{ShipRole::Interceptor, homePosition, std::nullopt});
                }
                break;
            }
        }
    }

    return goals;
}

}  // namespace astroparty::ai
