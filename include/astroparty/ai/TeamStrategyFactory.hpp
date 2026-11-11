#pragma once

#include <memory>
#include <string>

#include "astroparty/ai/TeamStrategy.hpp"

namespace astroparty::ai {

// Selects the offensive "strategic" policy, defensive "red" policy, or
// fixed-role "baseline". Both game entry points use the same names.
[[nodiscard]] std::shared_ptr<ITeamStrategy> makeTeamStrategyByName(const std::string& name);

}  // namespace astroparty::ai
