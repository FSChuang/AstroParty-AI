#pragma once

#include "astroparty/input/ShipController.hpp"

namespace astroparty::input {

// A deterministic controller that replays a fixed, repeating pattern of
// actions, entirely independent of `world` and of wall-clock time.
//
// This is intentionally NOT an AI: it makes no decisions based on game
// state. It exists so the headless runner and the controller/determinism
// tests have a controller that needs no human input and no SDL dependency,
// and whose output for a given tick count is always exactly reproducible.
//
// Behavior: turns right on every other tick (if `turnEveryOtherTick` is
// true) and fires once every `shootIntervalTicks` ticks (if positive, else
// never shoots).
class ScriptedShipController final : public IShipController {
public:
    ScriptedShipController(bool turnEveryOtherTick, int shootIntervalTicks) noexcept;

    [[nodiscard]] core::ShipAction decide(const core::World& world, int controlledShipId) override;

private:
    bool turnEveryOtherTick_;
    int shootIntervalTicks_;
    int tickCounter_ = 0;
};

}  // namespace astroparty::input
