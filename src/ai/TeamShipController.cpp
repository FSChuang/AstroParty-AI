#include "astroparty/ai/TeamShipController.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

#include "astroparty/ai/UnitBehaviors.hpp"
#include "astroparty/physics/Arena.hpp"

namespace astroparty::ai {

namespace {
// How far an Interceptor or Escort will fire at its target from. Chosen by
// eye; large enough that it starts shooting well before closing to point-blank.
constexpr float kAttackEngagementRange = 300.0f;
constexpr float kSelfDefenseRange = 125.0f;
constexpr float kShotIntervalSeconds = 0.35f;
constexpr float kEscortSupportRange = 230.0f;
constexpr float kEscortImmediateRange = 150.0f;

std::optional<core::Vector2> escortScreen(const core::World& world, const entities::Ship& self, const ShipGoal& goal) {
    if (!goal.protectedShipId) {
        return std::nullopt;
    }
    const auto allyPosition = world.ships()[*goal.protectedShipId].position();
    if (core::distanceSquared(self.position(), allyPosition) <= kEscortSupportRange * kEscortSupportRange ||
        core::distanceSquared(self.position(), goal.targetPosition) <= kEscortImmediateRange * kEscortImmediateRange) {
        return std::nullopt;
    }
    const auto screen = allyPosition + ((goal.targetPosition - allyPosition) * 0.5f).clampedToLength(65.0f);
    return physics::clampToArena(screen, world.arenaSize(), world.config().shipCollisionRadius);
}

bool allyInFiringLane(const core::World& world, const entities::Ship& self, const std::vector<int>& allies,
                     core::Vector2 targetPosition) {
    const auto forward = core::Vector2::fromAngleDegrees(self.headingDegrees());
    const float radius = world.config().shipCollisionRadius;
    const float shotDistance = (targetPosition - self.position()).length() + radius;
    for (const int id : allies) {
        const auto& ally = world.ships()[id];
        if (ally.id() == self.id() || !ally.isAlive()) {
            continue;
        }
        const auto offset = ally.position() - self.position();
        const float ahead = offset.x * forward.x + offset.y * forward.y;
        if (ahead >= world.config().bulletSpawnOffset - radius && ahead <= shotDistance &&
            (offset - forward * ahead).lengthSquared() <= radius * radius) {
            return true;
        }
    }
    return false;
}
}  // namespace

TeamShipController::TeamShipController(std::shared_ptr<ITeamStrategy> strategy, int teamId,
                                        std::vector<int> teamShipIds) noexcept
    : strategy_(std::move(strategy)), teamId_(teamId), teamShipIds_(std::move(teamShipIds)) {}

core::ShipAction TeamShipController::decide(const core::World& world, int controlledShipId) {
    const std::vector<ShipGoal> goals = strategy_->assignGoals(world, teamId_, teamShipIds_);

    const auto position = std::find(teamShipIds_.begin(), teamShipIds_.end(), controlledShipId);
    if (position == teamShipIds_.end()) {
        return core::ShipAction{};  // Misconfigured controller; fail safe rather than crash.
    }
    const int tick = world.elapsedTicks();
    if (lastShotTick_ && tick < *lastShotTick_) {
        lastShotTick_.reset();
    }
    const auto index = static_cast<std::size_t>(std::distance(teamShipIds_.begin(), position));
    const ShipGoal& goal = goals[index];
    lastRole_ = goal.role;

    const entities::Ship& self = world.ships()[static_cast<std::size_t>(controlledShipId)];
    const std::vector<core::ObstacleDefinition>& obstacles = world.obstacles();

    switch (goal.role) {
        case ShipRole::Interceptor:
        case ShipRole::SelfDefense:
        case ShipRole::Escort: {
            const auto velocity = goal.targetShipId ? world.ships()[*goal.targetShipId].velocity() : core::Vector2{};
            const auto screen = escortScreen(world, self, goal);
            const float range = goal.role == ShipRole::SelfDefense ? kSelfDefenseRange : kAttackEngagementRange;
            auto action = screen ? moveToward(self, *screen, obstacles, world.config())
                                 : pursueAndAttack(self, goal.targetPosition, obstacles, world.config(), range, velocity);
            action.allowResourcePickup = false;
            const int shotInterval = std::max(2, static_cast<int>(std::ceil(kShotIntervalSeconds * world.config().tickRateHz)));
            action.shoot = self.isAlive() && action.shoot &&
                           (!lastShotTick_ || tick - *lastShotTick_ >= shotInterval) &&
                           !allyInFiringLane(world, self, teamShipIds_, goal.targetPosition);
            if (action.shoot) {
                lastShotTick_ = tick;
            }
            return action;
        }
        case ShipRole::Collector:
            if (goal.targetShipId) {
                return moveWhileAvoidingEnemy(self, goal.targetPosition, world.ships()[*goal.targetShipId],
                                              obstacles, world.config());
            }
            return moveToward(self, goal.targetPosition, obstacles, world.config());
        case ShipRole::Defender:
        default:
            return moveToward(self, goal.targetPosition, obstacles, world.config());
    }
}

}  // namespace astroparty::ai
