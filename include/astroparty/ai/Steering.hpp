#pragma once

#include <vector>

#include "astroparty/core/MapDefinition.hpp"
#include "astroparty/core/Vector2.hpp"

namespace astroparty::ai {

// Low-level geometry shared by every rule-based controller in this project.
// These are pure functions with no knowledge of Ship, World, or any
// particular role/strategy -- they only know about headings, points, and
// obstacles, so they can be unit-tested in isolation and reused by both the
// strategic and baseline AI.

// Returns the compass heading (in degrees, using this project's
// screen-angle convention -- see core::Vector2::fromAngleDegrees) pointing
// from `from` toward `to`. Returns 0 if the two points coincide (no
// well-defined direction).
[[nodiscard]] float headingToward(core::Vector2 from, core::Vector2 to) noexcept;

// Ships in this game can only turn clockwise (there is no turn-left
// control -- see ShipAction::turnRight), so "how aligned am I with a
// target heading" is always a one-directional sweep, never a signed
// left/right difference. Returns the clockwise rotation, in degrees within
// [0, 360), that `currentHeadingDegrees` would need to sweep through to
// reach `targetHeadingDegrees`.
[[nodiscard]] float clockwiseRotationNeeded(float currentHeadingDegrees, float targetHeadingDegrees) noexcept;

// Decides whether to hold the turn button this step in order to (eventually)
// face `targetHeadingDegrees`, given turning is clockwise-only and happens
// at a fixed rate. Stops turning once within `toleranceDegrees` of the
// target, rather than overshooting back and forth every step.
[[nodiscard]] bool shouldTurnToFace(float currentHeadingDegrees, float targetHeadingDegrees,
                                    float toleranceDegrees) noexcept;

// If continuing straight ahead from `position` along `headingDegrees` would,
// within `lookaheadDistance`, bring a circle of `travelerRadius` into
// contact with any obstacle in `obstacles` (each padded by `clearance`),
// returns true. This is a simple reactive check, not path planning: callers
// that detect "something is in the way" via this function typically respond
// by turning (see unit behaviors), not by computing a route around it.
[[nodiscard]] bool isObstacleAhead(core::Vector2 position, float headingDegrees, float travelerRadius,
                                    const std::vector<core::ObstacleDefinition>& obstacles, float lookaheadDistance,
                                    float clearance) noexcept;

}  // namespace astroparty::ai
