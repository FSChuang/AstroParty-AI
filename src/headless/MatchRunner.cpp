#include "astroparty/headless/MatchRunner.hpp"

#include <stdexcept>

namespace astroparty::headless {

using core::MatchOutcome;
using core::ShipAction;
using core::World;
using input::IShipController;

MatchResult runMatch(World& world, std::vector<std::unique_ptr<IShipController>>& controllers, int maxTicks) {
    if (maxTicks <= 0) {
        throw std::invalid_argument("runMatch: maxTicks must be positive");
    }
    if (controllers.size() != world.ships().size()) {
        throw std::invalid_argument("runMatch: controllers.size() must equal world.ships().size()");
    }

    const float deltaSeconds = world.config().fixedDeltaSeconds();

    int tick = 0;
    for (; tick < maxTicks; ++tick) {
        if (world.matchState().hasEnded()) {
            break;
        }

        std::vector<ShipAction> actions;
        actions.reserve(controllers.size());
        for (std::size_t i = 0; i < controllers.size(); ++i) {
            actions.push_back(controllers[i]->decide(world, static_cast<int>(i)));
        }
        world.step(actions, deltaSeconds);
    }

    const MatchOutcome outcome = world.matchState().outcome();
    if (outcome == MatchOutcome::ShipWon) {
        return MatchResult{MatchResultOutcome::ShipWon, world.matchState().winningShipId(), tick};
    }
    if (outcome == MatchOutcome::Draw) {
        return MatchResult{MatchResultOutcome::Draw, std::nullopt, tick};
    }
    return MatchResult{MatchResultOutcome::Timeout, std::nullopt, tick};
}

}  // namespace astroparty::headless
