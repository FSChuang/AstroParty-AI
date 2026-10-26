#include "astroparty/core/SimulationConfig.hpp"

#include <fstream>
#include <sstream>
#include <stdexcept>

#include <nlohmann/json.hpp>

namespace astroparty::core {

namespace {

void requirePositive(float value, const char* fieldName) {
    if (!(value > 0.0f)) {
        throw std::invalid_argument(std::string("SimulationConfig.") + fieldName +
                                     " must be positive (got " + std::to_string(value) + ")");
    }
}

}  // namespace

void SimulationConfig::validate() const {
    requirePositive(arenaSize.x, "arenaSize.x");
    requirePositive(arenaSize.y, "arenaSize.y");
    requirePositive(tickRateHz, "tickRateHz");
    requirePositive(shipAccelerationUnitsPerSecondSquared, "shipAccelerationUnitsPerSecondSquared");
    requirePositive(shipMaxSpeedUnitsPerSecond, "shipMaxSpeedUnitsPerSecond");
    requirePositive(shipTurnRateDegreesPerSecond, "shipTurnRateDegreesPerSecond");
    requirePositive(shipCollisionRadius, "shipCollisionRadius");
    requirePositive(bulletSpeedUnitsPerSecond, "bulletSpeedUnitsPerSecond");
    requirePositive(bulletLifetimeSeconds, "bulletLifetimeSeconds");
    if (bulletSpawnOffset < 0.0f) {
        throw std::invalid_argument("SimulationConfig.bulletSpawnOffset must not be negative");
    }
    if (maxMatchTicks < 0) {
        throw std::invalid_argument("SimulationConfig.maxMatchTicks must not be negative");
    }
    // A ship wider than the arena can never be clamped to a valid position
    // (physics::clampToArena's precondition would be violated every step).
    if (shipCollisionRadius * 2.0f >= arenaSize.x || shipCollisionRadius * 2.0f >= arenaSize.y) {
        throw std::invalid_argument("SimulationConfig.shipCollisionRadius is too large for arenaSize");
    }
}

SimulationConfig SimulationConfig::loadFromFile(const std::filesystem::path& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        throw std::runtime_error("Could not open simulation config file: " + path.string());
    }

    nlohmann::json json;
    try {
        file >> json;
    } catch (const nlohmann::json::exception& error) {
        throw std::runtime_error("Malformed JSON in simulation config file " + path.string() + ": " + error.what());
    }

    SimulationConfig config;  // Start from Milestone-1-compatible defaults.

    if (json.contains("arenaWidth")) config.arenaSize.x = json.at("arenaWidth").get<float>();
    if (json.contains("arenaHeight")) config.arenaSize.y = json.at("arenaHeight").get<float>();
    if (json.contains("tickRateHz")) config.tickRateHz = json.at("tickRateHz").get<float>();
    if (json.contains("shipAccelerationUnitsPerSecondSquared")) {
        config.shipAccelerationUnitsPerSecondSquared = json.at("shipAccelerationUnitsPerSecondSquared").get<float>();
    }
    if (json.contains("shipMaxSpeedUnitsPerSecond")) {
        config.shipMaxSpeedUnitsPerSecond = json.at("shipMaxSpeedUnitsPerSecond").get<float>();
    }
    if (json.contains("shipTurnRateDegreesPerSecond")) {
        config.shipTurnRateDegreesPerSecond = json.at("shipTurnRateDegreesPerSecond").get<float>();
    }
    if (json.contains("shipCollisionRadius")) config.shipCollisionRadius = json.at("shipCollisionRadius").get<float>();
    if (json.contains("bulletSpeedUnitsPerSecond")) {
        config.bulletSpeedUnitsPerSecond = json.at("bulletSpeedUnitsPerSecond").get<float>();
    }
    if (json.contains("bulletLifetimeSeconds")) config.bulletLifetimeSeconds = json.at("bulletLifetimeSeconds").get<float>();
    if (json.contains("bulletSpawnOffset")) config.bulletSpawnOffset = json.at("bulletSpawnOffset").get<float>();
    if (json.contains("maxMatchTicks")) config.maxMatchTicks = json.at("maxMatchTicks").get<int>();

    config.validate();
    return config;
}

}  // namespace astroparty::core
