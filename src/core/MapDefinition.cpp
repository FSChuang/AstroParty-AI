#include "astroparty/core/MapDefinition.hpp"

#include <fstream>
#include <stdexcept>

#include <nlohmann/json.hpp>

namespace astroparty::core {

void MapDefinition::validate() const {
    if (arenaSize.x <= 0.0f || arenaSize.y <= 0.0f) {
        throw std::invalid_argument("MapDefinition '" + name + "': arenaSize must be positive");
    }
    if (shipSpawns.size() < 2) {
        throw std::invalid_argument("MapDefinition '" + name + "': at least two ship spawn points are required");
    }

    for (std::size_t i = 0; i < obstacles.size(); ++i) {
        if (obstacles[i].radius <= 0.0f) {
            throw std::invalid_argument("MapDefinition '" + name + "': obstacle " + std::to_string(i) +
                                         " must have a positive radius");
        }
    }

    const bool anySpawnHasTeam = [this] {
        for (const ShipSpawnPoint& spawn : shipSpawns) {
            if (spawn.teamId != -1) return true;
        }
        return false;
    }();

    for (std::size_t i = 0; i < shipSpawns.size(); ++i) {
        const ShipSpawnPoint& spawn = shipSpawns[i];
        if (spawn.position.x < 0.0f || spawn.position.x > arenaSize.x || spawn.position.y < 0.0f ||
            spawn.position.y > arenaSize.y) {
            throw std::invalid_argument("MapDefinition '" + name + "': spawn point " + std::to_string(i) +
                                         " lies outside the arena bounds");
        }
        if (anySpawnHasTeam && spawn.teamId == -1) {
            throw std::invalid_argument("MapDefinition '" + name + "': spawn point " + std::to_string(i) +
                                         " has no teamId, but other spawn points in this map do -- a map must"
                                         " either assign every ship to a team or none of them");
        }
        for (std::size_t j = 0; j < obstacles.size(); ++j) {
            const ObstacleDefinition& obstacle = obstacles[j];
            if (distanceSquared(spawn.position, obstacle.position) < obstacle.radius * obstacle.radius) {
                throw std::invalid_argument("MapDefinition '" + name + "': spawn point " + std::to_string(i) +
                                             " is placed inside obstacle " + std::to_string(j));
            }
        }
    }

    for (std::size_t i = 0; i < homeRegions.size(); ++i) {
        const HomeRegion& home = homeRegions[i];
        if (home.radius <= 0.0f) {
            throw std::invalid_argument("MapDefinition '" + name + "': home region " + std::to_string(i) +
                                         " must have a positive radius");
        }
        if (home.teamId < 0) {
            throw std::invalid_argument("MapDefinition '" + name + "': home region " + std::to_string(i) +
                                         " must have a non-negative teamId");
        }
    }

    for (std::size_t i = 0; i < resourceSpawnPoints.size(); ++i) {
        const Vector2& position = resourceSpawnPoints[i];
        if (position.x < 0.0f || position.x > arenaSize.x || position.y < 0.0f || position.y > arenaSize.y) {
            throw std::invalid_argument("MapDefinition '" + name + "': resource spawn point " + std::to_string(i) +
                                         " lies outside the arena bounds");
        }
    }
}

namespace {

Vector2 vectorFromJson(const nlohmann::json& json) { return Vector2{json.at("x").get<float>(), json.at("y").get<float>()}; }

}  // namespace

MapDefinition MapDefinition::loadFromFile(const std::filesystem::path& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        throw std::runtime_error("Could not open map file: " + path.string());
    }

    nlohmann::json json;
    try {
        file >> json;
    } catch (const nlohmann::json::exception& error) {
        throw std::runtime_error("Malformed JSON in map file " + path.string() + ": " + error.what());
    }

    MapDefinition map;
    try {
        map.name = json.value("name", path.stem().string());
        const auto& arena = json.at("arenaSize");
        map.arenaSize = vectorFromJson(arena);

        for (const auto& spawn : json.at("shipSpawns")) {
            ShipSpawnPoint point;
            point.position = vectorFromJson(spawn.at("position"));
            point.headingDegrees = spawn.value("headingDegrees", 0.0f);
            point.teamId = spawn.value("teamId", -1);
            map.shipSpawns.push_back(point);
        }

        if (json.contains("obstacles")) {
            for (const auto& obstacleJson : json.at("obstacles")) {
                ObstacleDefinition obstacle;
                obstacle.position = vectorFromJson(obstacleJson.at("position"));
                obstacle.radius = obstacleJson.at("radius").get<float>();
                map.obstacles.push_back(obstacle);
            }
        }

        if (json.contains("homeRegions")) {
            for (const auto& homeJson : json.at("homeRegions")) {
                HomeRegion home;
                home.teamId = homeJson.at("teamId").get<int>();
                home.position = vectorFromJson(homeJson.at("position"));
                home.radius = homeJson.at("radius").get<float>();
                map.homeRegions.push_back(home);
            }
        }

        if (json.contains("resourceSpawnPoints")) {
            for (const auto& resourceJson : json.at("resourceSpawnPoints")) {
                map.resourceSpawnPoints.push_back(vectorFromJson(resourceJson));
            }
        }
    } catch (const nlohmann::json::exception& error) {
        throw std::runtime_error("Map file " + path.string() + " is missing a required field: " + error.what());
    }

    map.validate();
    return map;
}

MapDefinition MapDefinition::defaultTwoShipMap() {
    // Ships spawn in opposite corners, angled toward the arena's center
    // rather than upstream's uniform 180-degree default (see
    // docs/UPSTREAM_AUDIT.md, section 10), so a
    // freshly started or restarted match reads clearly instead of both
    // ships immediately flying toward a screen edge.
    MapDefinition map;
    map.name = "default_two_ship";
    map.arenaSize = Vector2{1280.0f, 720.0f};
    map.shipSpawns = {
        ShipSpawnPoint{Vector2{120.0f, 120.0f}, 135.0f, -1},
        ShipSpawnPoint{Vector2{1160.0f, 600.0f}, 315.0f, -1},
    };
    return map;
}

}  // namespace astroparty::core
