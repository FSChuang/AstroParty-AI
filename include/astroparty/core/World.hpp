#pragma once

#include <vector>

#include "astroparty/core/Action.hpp"
#include "astroparty/core/MapDefinition.hpp"
#include "astroparty/core/MatchState.hpp"
#include "astroparty/core/SimulationConfig.hpp"
#include "astroparty/core/Vector2.hpp"
#include "astroparty/entities/Bullet.hpp"
#include "astroparty/entities/Ship.hpp"

namespace astroparty::core {

// Owns and steps the complete, renderer-independent simulation state for a
// single Astro Party match: a fixed roster of ships, their bullets, and the
// match's win/draw outcome.
//
// World has no knowledge of SDL, input devices, or networking, so it can be
// constructed, stepped, and inspected from a plain unit test or a headless
// batch-evaluation harness with no window at all -- this is the central fix
// for the simulation/rendering coupling identified in
// docs/UPSTREAM_AUDIT.md (item C-7).
//
// A World's ship roster is fixed for the whole match at whatever size the
// map provides; unlike upstream's dynamic, 1000-capacity player array (sized
// for a use case no current milestone has), eliminated ships are kept in
// place with isAlive() == false rather than removed, which keeps bullet
// ownership lookups and indices stable.
class World {
public:
    // Milestone-1-compatible convenience constructor: default config and the
    // built-in two-corner map (MapDefinition::defaultTwoShipMap()).
    World();

    // Constructs a World from an explicit, already-validated config and map.
    // `config.arenaSize` is overridden by `map.arenaSize` so the two never
    // disagree about the playfield bounds.
    World(SimulationConfig config, MapDefinition map);

    // Advances the simulation by deltaSeconds. actions[i] controls ships()[i];
    // if actions is shorter than ships(), the missing ships receive a
    // default (no-input) action. Has no effect once the match has ended --
    // call reset() to start a new one.
    void step(const std::vector<ShipAction>& actions, float deltaSeconds);

    // Restores the map's initial ship layout, clears all bullets, and resets
    // the match outcome to Undecided.
    void reset();

    [[nodiscard]] const std::vector<entities::Ship>& ships() const noexcept { return ships_; }
    [[nodiscard]] const std::vector<entities::Bullet>& bullets() const noexcept { return bullets_; }
    [[nodiscard]] const MatchState& matchState() const noexcept { return matchState_; }
    [[nodiscard]] const SimulationConfig& config() const noexcept { return config_; }
    [[nodiscard]] const MapDefinition& map() const noexcept { return map_; }
    [[nodiscard]] Vector2 arenaSize() const noexcept { return config_.arenaSize; }

private:
    SimulationConfig config_;
    MapDefinition map_;
    std::vector<entities::Ship> ships_;
    std::vector<entities::Bullet> bullets_;
    MatchState matchState_;

    void spawnInitialShips();
    void updateShips(const std::vector<ShipAction>& actions, float deltaSeconds);
    void updateBullets(float deltaSeconds);
    void resolveCollisions();
    void removeExpiredBullets();
};

}  // namespace astroparty::core
