#pragma once

#include "astroparty/core/Vector2.hpp"

// Gameplay tuning constants.
//
// Upstream CAstroParty (astro-party-workspace/audit/03_code_architecture.md,
// sections 3-6) advanced its simulation on an implicit fixed 60 Hz tick and
// applied these amounts directly, once per tick, with no delta-time scaling:
//
//   acceleration            = 0.3   units/tick
//   angular velocity        = 5     degrees/tick (while the turn button is held)
//   PLAYER_MAX_SPEED        = 4     units/tick
//   bullet speed            = 8     units/tick
//   BULLET_TIMER_TIMEOUT    = 180   ticks (3 seconds)
//
// This project drives its simulation from a measured, fixed delta time
// (see Application::run) rather than relying on an implicit call rate, so
// every "per tick" constant above is converted to a "per second" constant by
// multiplying by upstream's fixed rate of 60 ticks/second. World always
// steps with a 1/60 s delta time, so the per-step numeric effect is identical
// to upstream -- this preserves the original game feel while remaining
// correct if the tick rate is ever changed.
namespace astroparty::physics {

inline constexpr float kShipAccelerationUnitsPerSecondSquared = 18.0f;  // 0.3 * 60
inline constexpr float kShipMaxSpeedUnitsPerSecond = 240.0f;            // 4 * 60
inline constexpr float kShipTurnRateDegreesPerSecond = 300.0f;          // 5 * 60
inline constexpr float kBulletSpeedUnitsPerSecond = 480.0f;             // 8 * 60
inline constexpr float kBulletLifetimeSeconds = 3.0f;                   // 180 / 60

// Upstream derived its collision and boundary-clamp radius from the pixel
// dimensions of the loaded ship sprite. This project renders ships as vector
// primitives instead of textured sprites (Phase 2 instructions, section 4),
// so there is no texture to measure. kShipCollisionRadius is instead chosen
// to visually match the triangle GameRenderer draws and to give comfortable
// dodging room at the arena size below -- it is a deliberate replacement
// value, not a verified port of any upstream number.
inline constexpr float kShipCollisionRadius = 18.0f;

// Cosmetic radius used only when drawing a bullet. Bullets are treated as
// dimensionless points for collision purposes, matching upstream's
// Vector_in_radius2(player_position, bullet_position, kill_zone2).
inline constexpr float kBulletRenderRadius = 3.0f;

// Bullets spawn slightly ahead of the firing ship's nose so they are drawn
// outside the ship's own silhouette. Self-collision is also explicitly
// excluded in World::resolveCollisions, so this offset only affects visuals.
inline constexpr float kBulletSpawnOffset = kShipCollisionRadius + 10.0f;

// Matches upstream's WINDOW_WIDTH / WINDOW_HEIGHT.
inline constexpr core::Vector2 kArenaSize{1280.0f, 720.0f};

}  // namespace astroparty::physics
