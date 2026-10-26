#pragma once

#include <memory>
#include <string>
#include <vector>

#include "astroparty/core/World.hpp"
#include "astroparty/input/ShipController.hpp"

namespace astroparty::ai {

// Builds one IShipController per ship in `world` (which must be in
// GameMode::TeamResourceBattle), grouped into team 0 / team 1 by
// world.teamIdOfShip(), with every ship on a team sharing one strategy
// instance built by makeTeamStrategyByName(team{0,1}StrategyName). The
// result's size equals world.ships().size() and controllers[i] controls
// world.ships()[i], exactly what headless::runTeamMatch and Application
// both require. Shared by the headless and graphical entry points so a 3v3
// match is wired up identically regardless of which one is observing it.
[[nodiscard]] std::vector<std::unique_ptr<input::IShipController>> makeTeamControllers(
    const core::World& world, const std::string& team0StrategyName, const std::string& team1StrategyName);

}  // namespace astroparty::ai
