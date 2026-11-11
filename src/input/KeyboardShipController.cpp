#include "astroparty/input/KeyboardShipController.hpp"

#include <SDL2/SDL.h>

namespace astroparty::input {

KeyboardShipController::KeyboardShipController(SDL_Scancode turnLeftKey, SDL_Scancode turnRightKey,
                                               SDL_Scancode thrustKey, SDL_Scancode brakeKey,
                                               SDL_Scancode shootKey) noexcept
    : turnLeftKey_(turnLeftKey), turnRightKey_(turnRightKey), thrustKey_(thrustKey),
      brakeKey_(brakeKey), shootKey_(shootKey) {}

core::ShipAction KeyboardShipController::decide(const core::World& /*world*/, int /*controlledShipId*/) {
    const Uint8* keyboardState = SDL_GetKeyboardState(nullptr);
    return core::ShipAction{
        .turnRight = keyboardState[turnRightKey_] != 0,
        .shoot = keyboardState[shootKey_] != 0,
        .turnLeft = keyboardState[turnLeftKey_] != 0,
        .throttle = static_cast<float>(keyboardState[thrustKey_] != 0)
                  - static_cast<float>(keyboardState[brakeKey_] != 0),
    };
}

}  // namespace astroparty::input
