#pragma once

namespace astroparty::core {

// The actions a single ship can take during one simulation step.
//
// This is the sole contract between "something that decides what a ship
// should do" and the simulation itself. Human input (input::KeyboardShipController)
// produces a ShipAction today; a future AI controller only needs to produce
// the same structure to control a ship, with no change to World or Ship.
struct ShipAction {
    // Simultaneous left and right input cancels.
    bool turnRight = false;

    bool shoot = false;
    bool turnLeft = false;

    // Negative brakes along the current velocity, zero coasts, and positive
    // applies forward thrust. The simulation clamps the value to [-1, 1].
    float throttle = 0.0f;

    // Combat roles can pass over a core without becoming its carrier.
    bool allowResourcePickup = true;
};

}  // namespace astroparty::core
