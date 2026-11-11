#pragma once

#include "astroparty/ai/UtilityTeamStrategy.hpp"

namespace astroparty::ai {

// Red uses the shared role allocator with wider escort coverage and a
// reserve ship that screens home until an enemy is close enough to engage.
class RedTeamStrategy final : public UtilityTeamStrategy {
public:
    RedTeamStrategy() : UtilityTeamStrategy(320.0f) {}

private:
    [[nodiscard]] ShipGoal supportGoal(const core::World& world, int teamId, const entities::Ship& ship,
                                      core::Vector2 homePosition, std::optional<int> enemyCarrierId) const override;
};

}  // namespace astroparty::ai
