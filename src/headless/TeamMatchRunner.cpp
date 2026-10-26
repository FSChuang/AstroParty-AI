#include "astroparty/headless/TeamMatchRunner.hpp"

#include <stdexcept>

namespace astroparty::headless {

using core::GameMode;
using core::ShipAction;
using core::TeamMatchOutcome;
using core::World;
using input::IShipController;

TeamMatchResult runTeamMatch(World& world, std::vector<std::unique_ptr<IShipController>>& controllers,
                              int maxTicks) {
    if (maxTicks <= 0) {
        throw std::invalid_argument("runTeamMatch: maxTicks must be positive");
    }
    if (world.mode() != GameMode::TeamResourceBattle) {
        throw std::invalid_argument("runTeamMatch: world must be in GameMode::TeamResourceBattle");
    }
    if (controllers.size() != world.ships().size()) {
        throw std::invalid_argument("runTeamMatch: controllers.size() must equal world.ships().size()");
    }

    const float deltaSeconds = world.config().fixedDeltaSeconds();

    int tick = 0;
    for (; tick < maxTicks; ++tick) {
        if (world.teamMatchState().hasEnded()) {
            break;
        }

        std::vector<ShipAction> actions;
        actions.reserve(controllers.size());
        for (std::size_t i = 0; i < controllers.size(); ++i) {
            actions.push_back(controllers[i]->decide(world, static_cast<int>(i)));
        }
        world.step(actions, deltaSeconds);
    }

    TeamMatchResult result;
    result.ticksElapsed = tick;
    result.teamScores = world.teamScores();
    result.resourcesCollected = world.resourcesCollected();
    result.shipsDestroyed = world.shipsDestroyed();

    const TeamMatchOutcome outcome = world.teamMatchState().outcome();
    if (outcome == TeamMatchOutcome::TeamWon) {
        result.outcome = TeamMatchResultOutcome::TeamWon;
        result.winningTeamId = world.teamMatchState().winningTeamId();
    } else if (outcome == TeamMatchOutcome::Draw) {
        result.outcome = TeamMatchResultOutcome::Draw;
    } else {
        result.outcome = TeamMatchResultOutcome::Timeout;
    }
    return result;
}

}  // namespace astroparty::headless
