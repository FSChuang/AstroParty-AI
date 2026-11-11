#include "astroparty/ai/TeamStrategyFactory.hpp"

#include <stdexcept>

#include "astroparty/ai/BaselineTeamStrategy.hpp"
#include "astroparty/ai/RedTeamStrategy.hpp"
#include "astroparty/ai/UtilityTeamStrategy.hpp"

namespace astroparty::ai {

std::shared_ptr<ITeamStrategy> makeTeamStrategyByName(const std::string& name) {
    if (name == "strategic") {
        return std::make_shared<UtilityTeamStrategy>();
    }
    if (name == "baseline") {
        return std::make_shared<BaselineTeamStrategy>();
    }
    if (name == "red") {
        return std::make_shared<RedTeamStrategy>();
    }
    throw std::invalid_argument("Unknown team strategy '" + name + "': expected 'strategic', 'red', or 'baseline'");
}

}  // namespace astroparty::ai
