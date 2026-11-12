#include "astroparty/ai/TeamStrategyFactory.hpp"

#include <stdexcept>

#include "astroparty/ai/BaselineTeamStrategy.hpp"
#include "astroparty/ai/FixedRoleTeamStrategy.hpp"
#include "astroparty/ai/HysteresisTeamStrategy.hpp"
#include "astroparty/ai/UtilityTeamStrategy.hpp"

namespace astroparty::ai {

std::shared_ptr<ITeamStrategy> makeTeamStrategyByName(const std::string& name) {
    if (name == "strategic") {
        return std::make_shared<UtilityTeamStrategy>();
    }
    if (name == "baseline") {
        return std::make_shared<BaselineTeamStrategy>();
    }
    if (name == "fixed") {
        return std::make_shared<FixedRoleTeamStrategy>();
    }
    if (name == "hysteresis") {
        return std::make_shared<HysteresisTeamStrategy>();
    }
    throw std::invalid_argument("Unknown team strategy '" + name +
                                 "': expected 'strategic', 'baseline', 'fixed', or 'hysteresis'");
}

}  // namespace astroparty::ai
