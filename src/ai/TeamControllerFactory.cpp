#include "astroparty/ai/TeamControllerFactory.hpp"

#include "astroparty/ai/TeamShipController.hpp"
#include "astroparty/ai/TeamStrategyFactory.hpp"

namespace astroparty::ai {

std::vector<std::unique_ptr<input::IShipController>> makeTeamControllers(const core::World& world,
                                                                           const std::string& team0StrategyName,
                                                                           const std::string& team1StrategyName) {
    std::vector<int> team0Ships;
    std::vector<int> team1Ships;
    for (std::size_t i = 0; i < world.ships().size(); ++i) {
        (world.teamIdOfShip(static_cast<int>(i)) == 0 ? team0Ships : team1Ships).push_back(static_cast<int>(i));
    }

    const auto team0Strategy = makeTeamStrategyByName(team0StrategyName);
    const auto team1Strategy = makeTeamStrategyByName(team1StrategyName);

    std::vector<std::unique_ptr<input::IShipController>> controllers(world.ships().size());
    for (const int shipId : team0Ships) {
        controllers[static_cast<std::size_t>(shipId)] = std::make_unique<TeamShipController>(team0Strategy, 0, team0Ships);
    }
    for (const int shipId : team1Ships) {
        controllers[static_cast<std::size_t>(shipId)] = std::make_unique<TeamShipController>(team1Strategy, 1, team1Ships);
    }
    return controllers;
}

}  // namespace astroparty::ai
