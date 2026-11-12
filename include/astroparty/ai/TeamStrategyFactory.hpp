#pragma once

#include <memory>
#include <string>

#include "astroparty/ai/TeamStrategy.hpp"

namespace astroparty::ai {

// Builds a team strategy by name. Shared by the headless and graphical
// entry points (and the Milestone 5 experiment pipeline) so every caller
// selects strategies the exact same way. Throws std::invalid_argument for
// any other name.
//
//   "strategic"  -- UtilityTeamStrategy: this project's original dynamic,
//                   recomputed-every-tick utility-based assignment (see
//                   docs/ARCHITECTURE.md's "Team-level strategic AI"
//                   section). Unchanged since Milestone 3.
//   "baseline"   -- BaselineTeamStrategy: the simpler fixed-role-by-index
//                   opponent with a reactive Defender, used since
//                   Milestone 3 as the primary comparison point (see
//                   docs/ARCHITECTURE.md's "Baseline opponent" section).
//                   Unchanged.
//   "fixed"      -- FixedRoleTeamStrategy (Milestone 5): a literally fixed
//                   Collector/Defender/Interceptor assignment by ship
//                   index, never reassigned for strategic reasons.
//   "hysteresis" -- HysteresisTeamStrategy (Milestone 5): UtilityTeamStrategy's
//                   same priority logic, with Interceptor/Escort role
//                   commitment (a minimum hold duration plus a wider exit
//                   threshold) to resist threshold oscillation -- see
//                   docs/AI_EVALUATION.md.
[[nodiscard]] std::shared_ptr<ITeamStrategy> makeTeamStrategyByName(const std::string& name);

}  // namespace astroparty::ai
