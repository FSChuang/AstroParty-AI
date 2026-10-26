#pragma once

#include "astroparty/core/Vector2.hpp"

namespace astroparty::physics {

// Returns true when `point` lies within `radius` of `center`.
//
// Bullets are treated as dimensionless points and ships as circles, matching
// upstream's collision model (Vector_in_radius2 in object_handler.c; see
// astro-party-workspace/audit/03_code_architecture.md, section 6).
[[nodiscard]] bool isPointWithinRadius(core::Vector2 point, core::Vector2 center, float radius) noexcept;

}  // namespace astroparty::physics
