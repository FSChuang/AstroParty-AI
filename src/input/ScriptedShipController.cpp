#include "astroparty/input/ScriptedShipController.hpp"

namespace astroparty::input {

ScriptedShipController::ScriptedShipController(bool turnEveryOtherTick, int shootIntervalTicks) noexcept
    : turnEveryOtherTick_(turnEveryOtherTick), shootIntervalTicks_(shootIntervalTicks) {}

core::ShipAction ScriptedShipController::decide(const core::World& /*world*/, int /*controlledShipId*/) {
    core::ShipAction action;
    if (turnEveryOtherTick_) {
        action.turnRight = (tickCounter_ % 2) == 0;
    }
    if (shootIntervalTicks_ > 0) {
        action.shoot = (tickCounter_ % shootIntervalTicks_) == 0;
    }
    ++tickCounter_;
    return action;
}

}  // namespace astroparty::input
