#pragma once

#include <vector>

#include "astroparty/entities/Bullet.hpp"
#include "astroparty/entities/Ship.hpp"

namespace astroparty::core {

// Eliminates every ship within collision range of an enemy bullet, and
// immediately expires any bullet that lands a hit. A ship never takes
// damage from its own bullet (docs/UPSTREAM_AUDIT.md,
// section 6).
//
// This is World's hit-resolution rule, extracted as a standalone function so
// it can be exercised directly in unit tests with hand-placed ships and
// bullets, independent of any particular match's spawn layout or config.
void resolveShipBulletCollisions(std::vector<entities::Ship>& ships, std::vector<entities::Bullet>& bullets,
                                  float shipCollisionRadius) noexcept;

}  // namespace astroparty::core
