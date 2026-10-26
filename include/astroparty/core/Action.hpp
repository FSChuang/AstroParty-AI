#pragma once

namespace astroparty::core {

// The actions a single ship can take during one simulation step.
//
// This is the sole contract between "something that decides what a ship
// should do" and the simulation itself. Human input (input::KeyboardShipController)
// produces a ShipAction today; a future AI controller only needs to produce
// the same structure to control a ship, with no change to World or Ship.
struct ShipAction {
    // The ship only ever turns right (there is no turn-left control), which
    // matches the verified upstream mechanic -- see
    // docs/UPSTREAM_AUDIT.md, section 4.
    bool turnRight = false;

    bool shoot = false;
};

}  // namespace astroparty::core
