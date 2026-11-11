#include "astroparty/procgen/MapGeneratorConfig.hpp"

#include <fstream>
#include <stdexcept>
#include <string>

#include <nlohmann/json.hpp>

namespace astroparty::procgen {

namespace {

void requirePositive(float value, const char* fieldName) {
    if (!(value > 0.0f)) {
        throw std::invalid_argument(std::string("MapGeneratorConfig.") + fieldName + " must be positive (got " +
                                     std::to_string(value) + ")");
    }
}

void requirePositive(int value, const char* fieldName) {
    if (value <= 0) {
        throw std::invalid_argument(std::string("MapGeneratorConfig.") + fieldName + " must be positive (got " +
                                     std::to_string(value) + ")");
    }
}

}  // namespace

void MapGeneratorConfig::validate() const {
    requirePositive(arenaSize.x, "arenaSize.x");
    requirePositive(arenaSize.y, "arenaSize.y");
    requirePositive(shipsPerTeam, "shipsPerTeam");
    requirePositive(shipCollisionRadius, "shipCollisionRadius");
    requirePositive(homeRegionRadius, "homeRegionRadius");
    if (baseMarginFraction <= 0.0f || baseMarginFraction >= 0.5f) {
        throw std::invalid_argument("MapGeneratorConfig.baseMarginFraction must be in (0, 0.5)");
    }
    if (resourceCount <= 0) {
        throw std::invalid_argument("MapGeneratorConfig.resourceCount must be positive");
    }
    if (obstacleCount < 0) {
        throw std::invalid_argument("MapGeneratorConfig.obstacleCount must not be negative");
    }
    requirePositive(obstacleRadiusMin, "obstacleRadiusMin");
    requirePositive(obstacleRadiusMax, "obstacleRadiusMax");
    if (obstacleRadiusMin > obstacleRadiusMax) {
        throw std::invalid_argument("MapGeneratorConfig.obstacleRadiusMin must not exceed obstacleRadiusMax");
    }
    if (clusterCount < 0) {
        throw std::invalid_argument("MapGeneratorConfig.clusterCount must not be negative");
    }
    if (clusterCount > 0) {
        requirePositive(clusterSpreadRadius, "clusterSpreadRadius");
    }
    if (clusterCenterBandWidthFraction <= 0.0f || clusterCenterBandWidthFraction > 1.0f) {
        throw std::invalid_argument("MapGeneratorConfig.clusterCenterBandWidthFraction must be in (0, 1]");
    }
    if (minObstacleSpacing < 0.0f) {
        throw std::invalid_argument("MapGeneratorConfig.minObstacleSpacing must not be negative");
    }
    if (minBaseSeparationFromHome < 0.0f) {
        throw std::invalid_argument("MapGeneratorConfig.minBaseSeparationFromHome must not be negative");
    }
    requirePositive(maxPlacementAttemptsPerObject, "maxPlacementAttemptsPerObject");
}

namespace {
core::Vector2 vectorFromJson(const nlohmann::json& json) {
    return core::Vector2{json.at("x").get<float>(), json.at("y").get<float>()};
}
}  // namespace

MapGeneratorConfig MapGeneratorConfig::loadFromFile(const std::filesystem::path& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        throw std::runtime_error("Could not open generator config file: " + path.string());
    }

    nlohmann::json json;
    try {
        file >> json;
    } catch (const nlohmann::json::exception& error) {
        throw std::runtime_error("Malformed JSON in generator config file " + path.string() + ": " + error.what());
    }

    MapGeneratorConfig config;
    if (json.contains("arenaSize")) config.arenaSize = vectorFromJson(json.at("arenaSize"));
    if (json.contains("shipsPerTeam")) config.shipsPerTeam = json.at("shipsPerTeam").get<int>();
    if (json.contains("shipCollisionRadius")) config.shipCollisionRadius = json.at("shipCollisionRadius").get<float>();
    if (json.contains("homeRegionRadius")) config.homeRegionRadius = json.at("homeRegionRadius").get<float>();
    if (json.contains("baseMarginFraction")) config.baseMarginFraction = json.at("baseMarginFraction").get<float>();
    if (json.contains("resourceCount")) config.resourceCount = json.at("resourceCount").get<int>();
    if (json.contains("obstacleCount")) config.obstacleCount = json.at("obstacleCount").get<int>();
    if (json.contains("obstacleRadiusMin")) config.obstacleRadiusMin = json.at("obstacleRadiusMin").get<float>();
    if (json.contains("obstacleRadiusMax")) config.obstacleRadiusMax = json.at("obstacleRadiusMax").get<float>();
    if (json.contains("clusterCount")) config.clusterCount = json.at("clusterCount").get<int>();
    if (json.contains("clusterSpreadRadius")) config.clusterSpreadRadius = json.at("clusterSpreadRadius").get<float>();
    if (json.contains("clusterCenterBandWidthFraction")) {
        config.clusterCenterBandWidthFraction = json.at("clusterCenterBandWidthFraction").get<float>();
    }
    if (json.contains("minObstacleSpacing")) config.minObstacleSpacing = json.at("minObstacleSpacing").get<float>();
    if (json.contains("minBaseSeparationFromHome")) {
        config.minBaseSeparationFromHome = json.at("minBaseSeparationFromHome").get<float>();
    }
    if (json.contains("symmetric")) config.symmetric = json.at("symmetric").get<bool>();
    if (json.contains("maxPlacementAttemptsPerObject")) {
        config.maxPlacementAttemptsPerObject = json.at("maxPlacementAttemptsPerObject").get<int>();
    }

    config.validate();
    return config;
}

}  // namespace astroparty::procgen
