#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "astroparty/core/Vector2.hpp"

namespace astroparty::core {

struct ShipSpawnPoint {
    Vector2 position;
    float headingDegrees = 0.0f;
};

// Obstacles are currently DATA ONLY. MapDefinition can describe an
// obstacle's position and radius, and validate() checks its basic
// structural sanity, but World does not yet spawn a corresponding entity or
// collide anything against it -- that gameplay is planned for Milestone 3
// (astro-party-workspace/audit/06_implementation_roadmap.md). See
// docs/ARCHITECTURE.md for the current functional-vs-data-only breakdown of
// map features.
struct ObstacleDefinition {
    Vector2 position;
    float radius = 0.0f;
};

// A map: the arena's size, where ships start, and (for now, data-only)
// obstacle placements. Loading a map lets World's initial layout come from
// external data instead of the hardcoded two-corner spawn points Milestone 1
// used, which is what Milestone 3's fixed arenas and Milestone 4's
// procedural generation will both build on.
struct MapDefinition {
    std::string name;
    Vector2 arenaSize{1280.0f, 720.0f};
    std::vector<ShipSpawnPoint> shipSpawns;
    std::vector<ObstacleDefinition> obstacles;

    // Throws std::invalid_argument, naming the problem, if the map is
    // structurally invalid: fewer than two spawn points, a spawn point
    // outside the arena, or a non-positive obstacle radius.
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
