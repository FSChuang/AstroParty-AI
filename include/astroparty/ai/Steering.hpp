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

// Returns the clockwise rotation in degrees within [0, 360).
[[nodiscard]] float clockwiseRotationNeeded(float currentHeadingDegrees, float targetHeadingDegrees) noexcept;

// Shortest turn to the target. Positive is right, negative is left.
// An exact half turn chooses right.
[[nodiscard]] float signedRotationNeeded(float currentHeadingDegrees, float targetHeadingDegrees) noexcept;

// True when the heading error exceeds the alignment tolerance.
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
