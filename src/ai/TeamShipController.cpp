#include "astroparty/ai/TeamShipController.hpp"

#include <algorithm>
#include <utility>

#include "astroparty/ai/UnitBehaviors.hpp"

namespace astroparty::ai {

namespace {
// How far an Interceptor or Escort will fire at its target from. Chosen by
// eye; large enough that it starts shooting well before closing to point-blank.
constexpr float kAttackEngagementRange = 300.0f;
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
    const auto index = static_cast<std::size_t>(std::distance(teamShipIds_.begin(), position));
    const ShipGoal& goal = goals[index];

    const entities::Ship& self = world.ships()[static_cast<std::size_t>(controlledShipId)];
    const float shipCollisionRadius = world.config().shipCollisionRadius;
    const std::vector<core::ObstacleDefinition>& obstacles = world.obstacles();

    switch (goal.role) {
        case ShipRole::Interceptor:
        case ShipRole::Escort:
            return pursueAndAttack(self, goal.targetPosition, obstacles, shipCollisionRadius, kAttackEngagementRange);
        case ShipRole::Collector:
        case ShipRole::Defender:
        default:
            return moveToward(self, goal.targetPosition, obstacles, shipCollisionRadius);
    }
}

}  // namespace astroparty::ai
