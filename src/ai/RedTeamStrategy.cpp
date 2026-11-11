#include "astroparty/ai/RedTeamStrategy.hpp"

#include "astroparty/ai/TeamObservations.hpp"

namespace astroparty::ai {

namespace {
constexpr float kHomeDefenseRadius = 380.0f;
constexpr float kLocalEngagementRadius = 200.0f;
constexpr float kScreenFraction = 0.26f;
}

ShipGoal RedTeamStrategy::supportGoal(const core::World& world, int teamId, const entities::Ship& ship,
                                     core::Vector2 homePosition, std::optional<int> enemyCarrierId) const {
    const auto inRange = [&](int enemy, core::Vector2 position, float radius) {
        return core::distanceSquared(world.ships()[enemy].position(), position) <= radius * radius;
    };
    const auto attack = [&](int enemy) {
        return ShipGoal{ShipRole::Interceptor, world.ships()[enemy].position(), enemy};
    };
    if (const auto nearby = nearestAliveEnemyShipId(world, teamId, ship.position());
        nearby && inRange(*nearby, ship.position(), kLocalEngagementRadius)) {
        return attack(*nearby);
    }
    if (enemyCarrierId && inRange(*enemyCarrierId, homePosition, kHomeDefenseRadius)) {
        return attack(*enemyCarrierId);
    }
    if (const auto intruder = nearestAliveEnemyShipId(world, teamId, homePosition);
        intruder && inRange(*intruder, homePosition, kHomeDefenseRadius)) {
        return attack(*intruder);
    }
    const auto screen = homePosition + (world.arenaSize() * 0.5f - homePosition) * kScreenFraction;
    return ShipGoal{ShipRole::Defender, screen};
}

}  // namespace astroparty::ai
