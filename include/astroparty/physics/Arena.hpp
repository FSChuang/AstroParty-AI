#pragma once

#include "astroparty/core/Vector2.hpp"

namespace astroparty::physics {

// Confines `position` to an arena of size `arenaSize`, keeping it at least
// `margin` away from every edge. Used to keep a ship's circular silhouette
// fully inside the playfield, mirroring upstream's Vector_clamp() call in
// Player_update (astro-party-workspace/audit/03_code_architecture.md,
// section 4).
//
// Precondition: arenaSize.x > 2 * margin and arenaSize.y > 2 * margin, i.e.
// the arena must be large enough for the margin to leave a valid range. This
// always holds for the fixed kArenaSize / kShipCollisionRadius pairing
// defined in PhysicsConstants.hpp.
[[nodiscard]] core::Vector2 clampToArena(core::Vector2 position, core::Vector2 arenaSize, float margin) noexcept;

}  // namespace astroparty::physics
