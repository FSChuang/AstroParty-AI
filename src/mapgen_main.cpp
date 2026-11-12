// Entry point for the procedural map generator CLI: generates a batch of
// accepted Team Resource Battle maps from a MapGeneratorConfig and seed
// range, writes each as a loadable MapDefinition JSON file, and writes a
// manifest CSV (one row per accepted map, with its seed and computed
// metrics) that scripts/analyze_generated_maps.py consumes directly.
//
// This binary links only astroparty_sim -- never astroparty_platform or
// SDL2 -- exactly like AstroPartyAI-headless, so generating maps never
// requires a display.
#include <algorithm>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>

#include "astroparty/procgen/GenerationPipeline.hpp"
#include "astroparty/procgen/MapGeneratorConfig.hpp"
#include "astroparty/procgen/MapMetrics.hpp"

namespace {

struct CommandLineOptions {
    std::string configPath;
    std::uint32_t seed = 0;
    int count = 10;
    std::string outputDir;
    std::string label;
    int maxAttemptsMultiplier = 20;  // maxAttempts = count * this, unless --max-attempts overrides it.
    std::optional<int> maxAttempts;
    float cellSize = 18.0f;
    float clusterGapThreshold = 60.0f;
    int maxRepairRemovals = 5;
};

CommandLineOptions parseCommandLine(int argc, char** argv) {
    CommandLineOptions options;
    for (int i = 1; i < argc; ++i) {
        const std::string argument = argv[i];
        const bool hasNext = i + 1 < argc;

        if (argument == "--config" && hasNext) {
            options.configPath = argv[++i];
        } else if (argument == "--seed" && hasNext) {
            options.seed = static_cast<std::uint32_t>(std::stoul(argv[++i]));
        } else if (argument == "--count" && hasNext) {
            options.count = std::stoi(argv[++i]);
        } else if (argument == "--output-dir" && hasNext) {
            options.outputDir = argv[++i];
        } else if (argument == "--label" && hasNext) {
            options.label = argv[++i];
        } else if (argument == "--max-attempts" && hasNext) {
            options.maxAttempts = std::stoi(argv[++i]);
        } else if (argument == "--cell-size" && hasNext) {
            options.cellSize = std::stof(argv[++i]);
        } else if (argument == "--cluster-gap-threshold" && hasNext) {
            options.clusterGapThreshold = std::stof(argv[++i]);
        } else if (argument == "--max-repair-removals" && hasNext) {
            options.maxRepairRemovals = std::stoi(argv[++i]);
        } else {
            throw std::invalid_argument("Unrecognized or incomplete argument: " + argument);
        }
    }
    if (options.configPath.empty()) {
        throw std::invalid_argument("--config is required");
    }
    if (options.outputDir.empty()) {
        throw std::invalid_argument("--output-dir is required");
    }
    if (options.label.empty()) {
        options.label = std::filesystem::path(options.configPath).stem().string();
    }
    return options;
}

std::string zeroPadded(int value, int width) {
    std::ostringstream stream;
    stream << std::setw(width) << std::setfill('0') << value;
    return stream.str();
}

std::string csvField(const std::string& value) {
    // Every field this program writes is a config label, filename, or
    // number -- none contain commas or quotes in practice, but escape
    // defensively anyway so a future label with a comma cannot corrupt
    // the CSV's column structure.
    bool needsQuoting = value.find(',') != std::string::npos || value.find('"') != std::string::npos;
    if (!needsQuoting) {
        return value;
    }
    std::string escaped = "\"";
    for (char c : value) {
        if (c == '"') escaped += '"';
        escaped += c;
    }
    escaped += '"';
    return escaped;
}

}  // namespace

int main(int argc, char** argv) {
    try {
        const CommandLineOptions options = parseCommandLine(argc, argv);

        const astroparty::procgen::MapGeneratorConfig config =
            astroparty::procgen::MapGeneratorConfig::loadFromFile(options.configPath);

        const int maxAttempts = options.maxAttempts.value_or(options.count * options.maxAttemptsMultiplier);

        const astroparty::procgen::GenerationResult result =
            astroparty::procgen::generateMaps(config, options.seed, options.count, maxAttempts,
                                               config.shipCollisionRadius, options.cellSize, options.maxRepairRemovals);

        std::filesystem::create_directories(options.outputDir);

        std::ofstream manifest(std::filesystem::path(options.outputDir) / "manifest.csv");
        if (!manifest.is_open()) {
            throw std::runtime_error("Could not open manifest.csv for writing in " + options.outputDir);
        }
        manifest << "index,label,seed,filename,repaired,obstaclesRemovedByRepair,obstacleCount,resourceCount,"
                    "obstacleCoverageRatio,team0NavigationPathLength,team1NavigationPathLength,"
                    "teamAccessibilityImbalance,obstacleClusterCount,reachableFreeSpaceFraction\n";

        const int indexWidth = static_cast<int>(std::to_string(result.maps.size()).size());
        for (std::size_t i = 0; i < result.maps.size(); ++i) {
            const auto& record = result.maps[i];
            const std::string fileName = options.label + "_" + zeroPadded(static_cast<int>(i), std::max(indexWidth, 4)) + ".json";
            record.map.saveToFile(std::filesystem::path(options.outputDir) / fileName);

            const astroparty::procgen::MapMetrics metrics = astroparty::procgen::computeMapMetrics(
                record.map, config.shipCollisionRadius, options.cellSize, options.clusterGapThreshold);

            manifest << i << ',' << csvField(options.label) << ',' << record.seed << ',' << csvField(fileName) << ','
                      << (record.wasRepaired ? 1 : 0) << ',' << record.obstaclesRemovedByRepair << ','
                      << record.map.obstacles.size() << ',' << record.map.resourceSpawnPoints.size() << ','
                      << metrics.obstacleCoverageRatio << ',' << metrics.team0NavigationPathLength << ','
                      << metrics.team1NavigationPathLength << ',' << metrics.teamAccessibilityImbalance << ','
                      << metrics.obstacleClusterCount << ',' << metrics.reachableFreeSpaceFraction << '\n';
        }

        const auto& stats = result.stats;
        std::cout << "{\n"
                  << "  \"label\": \"" << options.label << "\",\n"
                  << "  \"totalAttempts\": " << stats.totalAttempts << ",\n"
                  << "  \"structurallyInvalid\": " << stats.structurallyInvalid << ",\n"
                  << "  \"geometricallyInvalid\": " << stats.geometricallyInvalid << ",\n"
                  << "  \"validRaw\": " << stats.validRaw << ",\n"
                  << "  \"repaired\": " << stats.repaired << ",\n"
                  << "  \"rejected\": " << stats.rejected << ",\n"
                  << "  \"accepted\": " << stats.accepted << ",\n"
                  << "  \"rawValidityRate\": " << stats.rawValidityRate() << ",\n"
                  << "  \"postRepairAcceptanceRate\": " << stats.postRepairAcceptanceRate() << "\n"
                  << "}\n";

        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "Fatal error: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
