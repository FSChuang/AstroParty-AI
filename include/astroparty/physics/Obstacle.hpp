#pragma once

#include "astroparty/core/Vector2.hpp"

namespace astroparty::physics {

// Returns true when two circles (centerA, radiusA) and (centerB, radiusB)
// overlap. Used for solid-body collision (ships vs. obstacles), as opposed
// to isPointWithinRadius() in Collision.hpp, which treats one side as a
// dimensionless point (bullets vs. ships or obstacles).
[[nodiscard]] bool circlesOverlap(core::Vector2 centerA, float radiusA, core::Vector2 centerB, float radiusB) noexcept;

// If a circle of `radius` centered at `position` overlaps a circle of
// `obstacleRadius` centered at `obstaclePosition`, returns `position` pushed
// directly away from `obstaclePosition` until the two circles are exactly
// tangent. Returns `position` unchanged if they do not overlap.
//
// This is a minimal, deliberately approximate collision *response* (not a
// physics engine): it corrects position only, leaving velocity untouched,
// so a ship whose heading points into an obstacle will be pushed back out
// every step -- visually, it "slides" along the obstacle's edge until it
// turns away. If a ship overlaps two obstacles at once, resolving against
// each in sequence may not perfectly satisfy both in a single step; this is
// an accepted approximation (see docs/ARCHITECTURE.md) rather than a reason
// to introduce a general-purpose physics engine for two obstacles per map.
//
// When `position == obstaclePosition` exactly (a degenerate case with no
// well-defined push direction), an arbitrary direction is used so the
// result is still well-defined and finite.
[[nodiscard]] core::Vector2 pushOutOfCircle(core::Vector2 position, float radius, core::Vector2 obstaclePosition,
                                             float obstacleRadius) noexcept;

}  // namespace astroparty::physics
