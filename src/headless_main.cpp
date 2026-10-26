// Entry point for the headless match runner: plays one complete Astro
// Party-AI match with no SDL window, using deterministic scripted
// controllers, and prints or saves a structured JSON result.
//
// This binary links only astroparty_sim -- never astroparty_platform or
// SDL2 -- so it can run in any environment, including CI containers and
// automated batch-evaluation scripts, with no display attached.
#include <cstdlib>
#include <exception>
#include <fstream>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "astroparty/ai/TeamControllerFactory.hpp"
#include "astroparty/core/GameMode.hpp"
#include "astroparty/core/MapDefinition.hpp"
#include "astroparty/core/SimulationConfig.hpp"
#include "astroparty/core/World.hpp"
#include "astroparty/headless/MatchRunner.hpp"
#include "astroparty/headless/TeamMatchRunner.hpp"
#include "astroparty/input/ScriptedShipController.hpp"

namespace {

// Which strategy a team uses in a Team Resource Battle headless run.
// "strategic" is this project's real strategic AI (UtilityTeamStrategy);
// "baseline" is the simpler fixed-role opponent (BaselineTeamStrategy) --
// see docs/ARCHITECTURE.md's "Baseline opponent" section. Choosing the same
// value for both teams runs Strategic-vs-Strategic or Baseline-vs-Baseline;
// choosing different values runs the primary Strategic-vs-Baseline evaluation.
struct CommandLineOptions {
    std::optional<std::string> configPath;
    std::optional<std::string> mapPath;
    int maxTicks = 3600;  // 60 seconds of simulated time at the default 60 Hz tick rate.
    std::optional<std::string> outputPath;
    std::string team0Strategy = "strategic";
    std::string team1Strategy = "baseline";
};

CommandLineOptions parseCommandLine(int argc, char** argv) {
    CommandLineOptions options;
    for (int i = 1; i < argc; ++i) {
        const std::string argument = argv[i];
        const bool hasNext = i + 1 < argc;

        if (argument == "--config" && hasNext) {
            options.configPath = argv[++i];
        } else if (argument == "--map" && hasNext) {
            options.mapPath = argv[++i];
        } else if (argument == "--max-ticks" && hasNext) {
            options.maxTicks = std::stoi(argv[++i]);
        } else if (argument == "--output" && hasNext) {
            options.outputPath = argv[++i];
        } else if (argument == "--team0-strategy" && hasNext) {
            options.team0Strategy = argv[++i];
        } else if (argument == "--team1-strategy" && hasNext) {
            options.team1Strategy = argv[++i];
        } else {
            throw std::invalid_argument("Unrecognized or incomplete argument: " + argument);
        }
    }
    return options;
}

// Builds one deterministic controller per ship. Only the first ship turns;
// the rest fly straight until the arena boundary clamps them in place. The
// default two-ship map's spawns are point-reflections of each other through
// the arena center, so two identically turning ships would trace mirrored
// paths that never cross -- breaking that symmetry here makes a default
// headless run far more likely to actually reach a conclusion instead of
// running out the clock.
std::vector<std::unique_ptr<astroparty::input::IShipController>> makeDefaultControllers(std::size_t shipCount) {
    std::vector<std::unique_ptr<astroparty::input::IShipController>> controllers;
    controllers.reserve(shipCount);
    for (std::size_t i = 0; i < shipCount; ++i) {
        const bool turnEveryOtherTick = (i == 0);
        const int shootIntervalTicks = 45 + static_cast<int>(i) * 10;
        controllers.push_back(
            std::make_unique<astroparty::input::ScriptedShipController>(turnEveryOtherTick, shootIntervalTicks));
    }
    return controllers;
}

}  // namespace

int main(int argc, char** argv) {
    try {
        const CommandLineOptions options = parseCommandLine(argc, argv);

        const astroparty::core::SimulationConfig config = options.configPath
                                                                ? astroparty::core::SimulationConfig::loadFromFile(*options.configPath)
                                                                : astroparty::core::SimulationConfig{};

        const astroparty::core::MapDefinition map = options.mapPath
                                                          ? astroparty::core::MapDefinition::loadFromFile(*options.mapPath)
                                                          : astroparty::core::MapDefinition::defaultTwoShipMap();

        astroparty::core::World world(config, map);

        std::string json;
        if (world.mode() == astroparty::core::GameMode::TeamResourceBattle) {
            auto controllers = astroparty::ai::makeTeamControllers(world, options.team0Strategy, options.team1Strategy);
            const astroparty::headless::TeamMatchResult result =
                astroparty::headless::runTeamMatch(world, controllers, options.maxTicks);
            json = result.toJsonString();
        } else {
            auto controllers = makeDefaultControllers(world.ships().size());
            const astroparty::headless::MatchResult result = astroparty::headless::runMatch(world, controllers, options.maxTicks);
            json = result.toJsonString();
        }

        if (options.outputPath) {
            std::ofstream outputFile(*options.outputPath);
            if (!outputFile.is_open()) {
                throw std::runtime_error("Could not open output file: " + *options.outputPath);
            }
            outputFile << json << '\n';
        } else {
            std::cout << json << '\n';
        }

        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "Fatal error: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
