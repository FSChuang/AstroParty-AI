#pragma once

#include <filesystem>

#include "astroparty/core/Vector2.hpp"

namespace astroparty::core {

// All tunable simulation parameters, grouped so that a match's rules are
// fully determined by one explicit, inspectable value rather than scattered
// global constants.
//
// The default values below exactly reproduce Milestone 1's original
// hardcoded behavior. They are derived from upstream CAstroParty's per-tick
// constants at an implicit 60 Hz; see
// astro-party-workspace/audit/03_code_architecture.md (sections 3-6) for the
// upstream values and docs/ARCHITECTURE.md for the unit conversion. Changing
// these is now a configuration decision, not a code change.
struct SimulationConfig {
    Vector2 arenaSize{1280.0f, 720.0f};
    float tickRateHz = 60.0f;

    float shipAccelerationUnitsPerSecondSquared = 18.0f;
    float shipMaxSpeedUnitsPerSecond = 240.0f;
    float shipTurnRateDegreesPerSecond = 300.0f;
    float shipCollisionRadius = 18.0f;

    float bulletSpeedUnitsPerSecond = 480.0f;
    float bulletLifetimeSeconds = 3.0f;
    // Bullets spawn this far ahead of the firing ship's nose so they are
    // drawn outside the ship's own silhouette. Self-collision is also
    // explicitly excluded in resolveShipBulletCollisions, so this only
    // affects where a bullet first appears, not collision outcomes.
    float bulletSpawnOffset = 28.0f;

    // 0 means "no limit," appropriate for the graphical app, where a human
    // ends the session by closing the window. A headless run always
    // supplies an explicit positive cap (see headless::runMatch).
    int maxMatchTicks = 0;

    [[nodiscard]] float fixedDeltaSeconds() const { return 1.0f / tickRateHz; }

    // Throws std::invalid_argument, naming the offending field, if any
    // value is physically nonsensical (non-positive size, speed, rate, or
    // tick rate).
    void validate() const;

    // Parses a SimulationConfig from a JSON file. Fields absent from the
    // file keep their Milestone-1-compatible default above, so a config
    // file only needs to mention the parameters it actually changes.
    // Throws std::runtime_error if the file cannot be read or is not valid
    // JSON, and std::invalid_argument (via validate()) if parsed values are
    // invalid.
    [[nodiscard]] static SimulationConfig loadFromFile(const std::filesystem::path& path);
};

}  // namespace astroparty::core
