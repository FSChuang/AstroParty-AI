#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "astroparty/core/Vector2.hpp"

namespace astroparty::core {

struct ShipSpawnPoint {
    Vector2 position;
    float headingDegrees = 0.0f;

    // -1 means "no team": used by Classic Deathmatch maps, where ships are
    // free-for-all and team membership is meaningless. A Team Resource
    // Battle map assigns every spawn to team 0 or team 1 instead.
    int teamId = -1;
};

// An obstacle is a solid, static, circular object: ships are pushed out of
// it (see physics::pushOutOfCircle) and bullets that touch it are destroyed.
// It never moves and cannot be destroyed itself.
struct ObstacleDefinition {
    Vector2 position;
    float radius = 0.0f;
};

// Where a team can deliver carried resources to score. Only meaningful for
// Team Resource Battle maps.
struct HomeRegion {
    int teamId = -1;
    Vector2 position;
    float radius = 0.0f;
};

// A map: the arena's size, where ships start (optionally with team
// assignments), obstacle placements, and -- for Team Resource Battle maps --
// home regions and resource spawn points. Loading a map lets World's
// initial layout come from external data instead of hardcoded positions,
// which is what fixed arenas today and procedural generation in a later
// milestone both build on.
struct MapDefinition {
    std::string name;
    Vector2 arenaSize{1280.0f, 720.0f};
    std::vector<ShipSpawnPoint> shipSpawns;
    std::vector<ObstacleDefinition> obstacles;
    std::vector<HomeRegion> homeRegions;
    std::vector<Vector2> resourceSpawnPoints;

    // Throws std::invalid_argument, naming the problem, if the map is
    // structurally invalid: fewer than two spawn points, a spawn point
    // outside the arena, a spawn point overlapping an obstacle, a
    // non-positive obstacle or home region radius, a resource spawn point
    // outside the arena, or an inconsistent mix of team-assigned and
    // unassigned ship spawns.
    void validate() const;

    // Parses a MapDefinition from a JSON file. Throws std::runtime_error if
    // the file cannot be read or is not valid JSON, and
    // std::invalid_argument (via validate()) if the parsed map is invalid.
    [[nodiscard]] static MapDefinition loadFromFile(const std::filesystem::path& path);

    // The built-in two-corner layout Milestone 1 used before map loading
    // existed. World's default constructor uses this, so existing behavior
    // and tests are unaffected by map loading's introduction.
    [[nodiscard]] static MapDefinition defaultTwoShipMap();
};

}  // namespace astroparty::core
