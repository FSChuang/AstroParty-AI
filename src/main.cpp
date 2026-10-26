#include <cstdlib>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>

#include "Application.hpp"

namespace {

// Simple CLI selection, deliberately no menu system (see
// ApplicationOptions): `--mode team` observes a 3v3 Team Resource Battle AI
// match instead of the default two-player Classic Deathmatch.
astroparty::ApplicationOptions parseCommandLine(int argc, char** argv) {
    astroparty::ApplicationOptions options;
    for (int i = 1; i < argc; ++i) {
        const std::string argument = argv[i];
        const bool hasNext = i + 1 < argc;

        if (argument == "--mode" && hasNext) {
            const std::string mode = argv[++i];
            if (mode == "classic") {
                options.mode = astroparty::core::GameMode::ClassicDeathmatch;
            } else if (mode == "team") {
                options.mode = astroparty::core::GameMode::TeamResourceBattle;
            } else {
                throw std::invalid_argument("Unknown --mode '" + mode + "': expected 'classic' or 'team'");
            }
        } else if (argument == "--config" && hasNext) {
            options.configPath = argv[++i];
        } else if (argument == "--map" && hasNext) {
            options.mapPath = argv[++i];
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

}  // namespace

int main(int argc, char** argv) {
    try {
        astroparty::Application application(parseCommandLine(argc, argv));
        return application.run();
    } catch (const std::exception& error) {
        std::cerr << "Fatal error: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
