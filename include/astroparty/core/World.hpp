#pragma once

#include <array>
#include <vector>

#include "astroparty/core/Action.hpp"
#include "astroparty/core/GameMode.hpp"
#include "astroparty/core/MapDefinition.hpp"
#include "astroparty/core/MatchState.hpp"
#include "astroparty/core/SimulationConfig.hpp"
#include "astroparty/core/TeamMatchState.hpp"
#include "astroparty/core/Vector2.hpp"
#include "astroparty/entities/Bullet.hpp"
#include "astroparty/entities/Resource.hpp"
#include "astroparty/entities/Ship.hpp"

namespace astroparty::core {

// Owns and steps the complete, renderer-independent simulation state for a
// single Astro Party match: ships, bullets, (for Team Resource Battle)
// resources and team scores, obstacles, and the match's outcome.
//
// World has no knowledge of SDL, input devices, or networking, so it can be
// constructed, stepped, and inspected from a plain unit test or a headless
// batch-evaluation harness with no window at all -- this is the central fix
// for the simulation/rendering coupling identified in
// docs/UPSTREAM_AUDIT.md (item C-7).
//
// Both game modes (see GameMode.hpp) share the exact same ship/bullet
// physics, arena bounds, and obstacle collision -- that code path is never
// duplicated between them. They differ only in: what happens to an
// eliminated ship (Classic Deathmatch leaves it dead; Team Resource Battle
// respawns it after config().respawnDelayTicks) and how the match's
// end is decided (matchState() for Classic Deathmatch; teamMatchState()
// for Team Resource Battle). Only the accessor matching the current mode()
// is meaningful; the other always reports Undecided.
//
// A World's ship roster is fixed for the whole match at whatever size the
// map provides; unlike upstream's dynamic, 1000-capacity player array (sized
// for a use case no current milestone has), eliminated ships are kept in
// place with isAlive() == false rather than removed, which keeps bullet
// ownership, team lookups, and respawn-timer indices stable.
class World {
public:
    // Milestone-1-compatible convenience constructor: default config
    // (Classic Deathmatch) and the built-in two-corner map.
    World();

    // Constructs a World from an explicit, already-validated config and map.
    // `config.arenaSize` is overridden by `map.arenaSize` so the two never
    // disagree about the playfield bounds. If config.mode is
    // TeamResourceBattle, throws std::invalid_argument unless `map`
    // provides exactly three ship spawns on each of team 0 and team 1, a
    // home region for each team, and at least one resource spawn point.
    World(SimulationConfig config, MapDefinition map);

    // Advances the simulation by deltaSeconds. actions[i] controls ships()[i];
    // if actions is shorter than ships(), the missing ships receive a
    // default (no-input) action. Has no effect once the match has ended --
    // call reset() to start a new one.
    void step(const std::vector<ShipAction>& actions, float deltaSeconds);

    // Restores the map's initial ship layout, clears all bullets, resets
    // resources/scores/respawn timers (unused but harmless in Classic
    // Deathmatch), and resets the match outcome to Undecided.
    void reset();

    [[nodiscard]] const std::vector<entities::Ship>& ships() const noexcept { return ships_; }
    [[nodiscard]] const std::vector<entities::Bullet>& bullets() const noexcept { return bullets_; }
    [[nodiscard]] const std::vector<ObstacleDefinition>& obstacles() const noexcept { return map_.obstacles; }
    [[nodiscard]] const std::vector<entities::Resource>& resources() const noexcept { return resources_; }
    [[nodiscard]] const MatchState& matchState() const noexcept { return matchState_; }
    [[nodiscard]] const TeamMatchState& teamMatchState() const noexcept { return teamMatchState_; }
    [[nodiscard]] const SimulationConfig& config() const noexcept { return config_; }
    [[nodiscard]] const MapDefinition& map() const noexcept { return map_; }
    [[nodiscard]] Vector2 arenaSize() const noexcept { return config_.arenaSize; }
    [[nodiscard]] GameMode mode() const noexcept { return config_.mode; }
    // Resources successfully delivered per team (what teamMatchState()'s
    // winningScore counts against).
    [[nodiscard]] const std::array<int, 2>& teamScores() const noexcept { return teamScores_; }

    // Cumulative resource pickups per team so far this match -- always >=
    // the corresponding teamScores() entry, since a resource dropped after
    // its carrier is eliminated and later picked up again counts as another
    // collection without necessarily having been delivered yet.
    [[nodiscard]] const std::array<int, 2>& resourcesCollected() const noexcept { return resourcesCollected_; }

    // Cumulative ship eliminations per team so far this match (a ship that
    // respawns and dies again counts twice).
    [[nodiscard]] const std::array<int, 2>& shipsDestroyed() const noexcept { return shipsDestroyed_; }

    [[nodiscard]] int elapsedTicks() const noexcept { return elapsedTicks_; }

    // The team ID (0 or 1) that ships()[shipId] belongs to, or -1 in
    // Classic Deathmatch / for a free-for-all spawn.
    [[nodiscard]] int teamIdOfShip(int shipId) const noexcept;

private:
    SimulationConfig config_;
    MapDefinition map_;
    std::vector<entities::Ship> ships_;
    std::vector<entities::Bullet> bullets_;
    std::vector<entities::Resource> resources_;
    MatchState matchState_;             // Meaningful only when mode() == ClassicDeathmatch.
    TeamMatchState teamMatchState_;      // Meaningful only when mode() == TeamResourceBattle.
    std::array<int, 2> teamScores_{};
    std::array<int, 2> resourcesCollected_{};
    std::array<int, 2> shipsDestroyed_{};
    std::vector<int> respawnTicksRemaining_;  // One per ship; 0 = not currently awaiting respawn.
    int elapsedTicks_ = 0;

    void validateTeamResourceBattleRequirements() const;
    void spawnInitialShips();
    void spawnInitialResources();
    void updateShips(const std::vector<ShipAction>& actions, float deltaSeconds);
    void updateBullets(float deltaSeconds);
    void resolveCollisions();
    void removeExpiredBullets();

    // Team Resource Battle-only orchestration. step() calls these only when
    // mode() == TeamResourceBattle: in particular, updateRespawns() must
    // never run in Classic Deathmatch, or an eliminated ship there would
    // incorrectly come back to life instead of staying dead for the rest of
    // the match.
    void dropResourcesCarriedByEliminatedShips();
    void updateResourcePickupAndDelivery();
    void updateRespawns();
    [[nodiscard]] bool isCarryingAnyResource(int shipId) const noexcept;
};

}  // namespace astroparty::core
