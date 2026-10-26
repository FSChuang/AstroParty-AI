#pragma once

#include "astroparty/core/Action.hpp"
#include "astroparty/core/World.hpp"

namespace astroparty::input {

// Decides the ShipAction for one ship, once per simulation step.
//
// Implemented today by KeyboardShipController (human input) and
// ScriptedShipController (a fixed, deterministic pattern used for headless
// runs and tests); a future AI controller only needs to implement this same
// interface -- World only ever sees the resulting ShipAction and never needs
// to know which kind of controller produced it.
//
// `world` is the authoritative game state as of the end of the previous
// step. Implementations may read it to inform their decision (e.g. a future
// AI controller aiming at an opponent) but must never mutate it: this
// interface only exposes World by const reference, so there is no way for a
// controller to change simulation state except through the ShipAction it
// returns. `controlledShipId` identifies which entry in world.ships() this
// controller is responsible for.
//
// decide() is deliberately not const on the controller itself: a scripted
// or learning controller is expected to advance its own internal state
// (e.g. a step counter) between calls, even though it may never touch
// `world`.
class IShipController {
public:
    virtual ~IShipController() = default;

    [[nodiscard]] virtual core::ShipAction decide(const core::World& world, int controlledShipId) = 0;
};

}  // namespace astroparty::input
