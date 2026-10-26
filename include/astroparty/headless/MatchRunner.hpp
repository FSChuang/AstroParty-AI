#pragma once

#include <memory>
#include <vector>

#include "astroparty/core/World.hpp"
#include "astroparty/headless/MatchResult.hpp"
#include "astroparty/input/ShipController.hpp"

namespace astroparty::headless {

// Runs a complete, renderer-free match: once per fixed timestep (taken from
// world.config().fixedDeltaSeconds()), asks each controller to decide() an
// action and steps `world` with the result, until the match ends or
// `maxTicks` fixed steps have run.
//
// controllers.size() must equal world.ships().size(); controllers[i]
// controls world.ships()[i]. No SDL initialization happens anywhere in this
// call -- only astroparty_sim types are touched -- so this can run in a
// process with no display, and can be called repeatedly (e.g. with a fresh
// World each time) for batch evaluation.
[[nodiscard]] MatchResult runMatch(core::World& world, std::vector<std::unique_ptr<input::IShipController>>& controllers,
                                    int maxTicks);

}  // namespace astroparty::headless
