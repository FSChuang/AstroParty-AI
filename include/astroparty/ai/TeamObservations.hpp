#pragma once

#include <optional>

#include "astroparty/core/Vector2.hpp"
#include "astroparty/core/World.hpp"

namespace astroparty::ai {

// Small, pure queries over World shared by every team strategy. Factoring
// these out keeps each strategy's assignGoals() focused on its actual
// decision logic rather than repeated world-scanning boilerplate -- and
// guarantees the strategic and baseline AI observe the game through
// identical queries, with no privileged information available to one but
// not the other (see docs/ARCHITECTURE.md's "Baseline opponent" section).

// The position of `teamId`'s home region, or std::nullopt if the current
// map defines none for that team (should not happen for a World
// successfully constructed in TeamResourceBattle mode, which requires one
// per team, but callers should not assume it is never missing).
[[nodiscard]] std::optional<core::Vector2> homeRegionPosition(const core::World& world, int teamId) noexcept;

// True if any resource is currently carried by ships()[shipId].
[[nodiscard]] bool isShipCarryingAnyResource(const core::World& world, int shipId) noexcept;

// The spawn position of the nearest currently-available (AtSpawn) resource
// to `from`, or std::nullopt if none are available.
[[nodiscard]] std::optional<core::Vector2> nearestAvailableResourcePosition(const core::World& world,
                                                                             core::Vector2 from) noexcept;

// The ship ID of the nearest alive ship NOT on `ownTeamId` to `from`, or
// std::nullopt if no enemy ship is alive.
[[nodiscard]] std::optional<int> nearestAliveEnemyShipId(const core::World& world, int ownTeamId,
                                                          core::Vector2 from) noexcept;

// The ship ID of an enemy (not on `ownTeamId`) currently carrying a
// resource, preferring whichever carrier is closest to delivering it (i.e.
// closest to its OWN home region), or std::nullopt if no enemy is carrying.
[[nodiscard]] std::optional<int> mostImminentEnemyCarrierShipId(const core::World& world, int ownTeamId) noexcept;

// The ship ID of a ship on `ownTeamId` currently carrying a resource, or
// std::nullopt if none are.
[[nodiscard]] std::optional<int> friendlyCarrierShipId(const core::World& world, int ownTeamId) noexcept;

}  // namespace astroparty::ai
