#pragma once

#include <memory>
#include <vector>

#include "astroparty/core/World.hpp"
#include "astroparty/headless/TeamMatchResult.hpp"
#include "astroparty/input/ShipController.hpp"

namespace astroparty::headless {

// Runs a complete, renderer-free Team Resource Battle match: once per fixed
// timestep, asks each controller to decide() an action and steps `world`
// with the result, until the match ends (world.teamMatchState().hasEnded())
// or `maxTicks` fixed steps have run.
//
// `world` must be in GameMode::TeamResourceBattle. controllers.size() must
// equal world.ships().size(); controllers[i] controls world.ships()[i] --
// typically a mix of ai::TeamShipController instances, one per ship, built
// from a shared ai::ITeamStrategy per team (see docs/ARCHITECTURE.md's
// headless evaluation section). No SDL initialization happens here, so this
// can run in a display-less process and be called repeatedly for batch
// evaluation.
[[nodiscard]] TeamMatchResult runTeamMatch(core::World& world,
                                            std::vector<std::unique_ptr<input::IShipController>>& controllers,
                                            int maxTicks);

}  // namespace astroparty::headless
