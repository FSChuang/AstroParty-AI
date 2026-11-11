#pragma once

#include <SDL2/SDL_scancode.h>

#include "astroparty/input/ShipController.hpp"

namespace astroparty::input {

// Maps a local player's turning, throttle, and shooting keys to a ShipAction.
//
// Construct one instance per locally controlled ship so two players can use
// disjoint key clusters on the same keyboard. Unlike upstream, which read
// mouse clicks for a single local player (see
// docs/UPSTREAM_AUDIT.md, section 8), this
// project needs two independent local players, which keyboard scancodes
// support directly.
class KeyboardShipController final : public IShipController {
public:
    KeyboardShipController(SDL_Scancode turnLeftKey, SDL_Scancode turnRightKey,
                           SDL_Scancode thrustKey, SDL_Scancode brakeKey, SDL_Scancode shootKey) noexcept;

    // Ignores `world` and `controlledShipId`: this controller always reads
    // live physical keyboard state, which has no notion of "which ship."
    [[nodiscard]] core::ShipAction decide(const core::World& world, int controlledShipId) override;

private:
    SDL_Scancode turnLeftKey_;
    SDL_Scancode turnRightKey_;
    SDL_Scancode thrustKey_;
    SDL_Scancode brakeKey_;
    SDL_Scancode shootKey_;
};

}  // namespace astroparty::input
