#pragma once

#include <vector>

#include "astroparty/core/MapDefinition.hpp"
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

// Pushes every alive ship out of any obstacle it overlaps, so ships cannot
// pass through solid terrain. See Ship::resolveObstacleOverlap and
// physics::pushOutOfCircle for the (deliberately approximate, non-physics-engine)
// resolution used.
void resolveShipObstacleCollisions(std::vector<entities::Ship>& ships, const std::vector<ObstacleDefinition>& obstacles,
                                    float shipCollisionRadius) noexcept;

// Immediately expires any bullet that overlaps an obstacle -- obstacles
// block line of fire. Bullets are treated as dimensionless points here, the
// same convention as resolveShipBulletCollisions.
void resolveBulletObstacleCollisions(std::vector<entities::Bullet>& bullets,
                                      const std::vector<ObstacleDefinition>& obstacles) noexcept;

}  // namespace astroparty::core
