#include "astroparty/input/KeyboardShipController.hpp"

#include <SDL2/SDL.h>

namespace astroparty::input {

KeyboardShipController::KeyboardShipController(SDL_Scancode turnRightKey, SDL_Scancode shootKey) noexcept
    : turnRightKey_(turnRightKey), shootKey_(shootKey) {}

core::ShipAction KeyboardShipController::decide(const core::World& /*world*/, int /*controlledShipId*/) {
    const Uint8* keyboardState = SDL_GetKeyboardState(nullptr);
    return core::ShipAction{
        .turnRight = keyboardState[turnRightKey_] != 0,
        .shoot = keyboardState[shootKey_] != 0,
    };
}

}  // namespace astroparty::input
