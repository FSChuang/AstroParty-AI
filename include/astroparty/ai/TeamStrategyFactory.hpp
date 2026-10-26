#pragma once

#include <memory>
#include <string>

#include "astroparty/ai/TeamStrategy.hpp"

namespace astroparty::ai {

// Builds a team strategy by name: "strategic" for UtilityTeamStrategy (this
// project's real strategic AI) or "baseline" for BaselineTeamStrategy (the
// simpler fixed-role opponent). Shared by the headless and graphical entry
// points so both select strategies the exact same way -- see
// docs/ARCHITECTURE.md's "Baseline opponent" section. Throws
// std::invalid_argument for any other name.
[[nodiscard]] std::shared_ptr<ITeamStrategy> makeTeamStrategyByName(const std::string& name);

}  // namespace astroparty::ai
