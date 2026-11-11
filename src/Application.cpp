#include "Application.hpp"

#include <SDL2/SDL.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <filesystem>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <system_error>
#include <utility>

#if defined(__unix__) || defined(__APPLE__)
#include <cerrno>
#include <cstdlib>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>

extern char** environ;
#endif

#include "astroparty/ai/TeamControllerFactory.hpp"
#include "astroparty/ai/TeamShipController.hpp"
#include "astroparty/core/MapDefinition.hpp"
#include "astroparty/core/SimulationConfig.hpp"
#include "astroparty/input/KeyboardShipController.hpp"

namespace astroparty {

namespace {

constexpr int kWindowWidth = 1280;
constexpr int kWindowHeight = 720;

// Caps how much simulation time a single frame can catch up on, so a long
// stall (e.g. the user drags the window, or a debugger pauses the process)
// cannot force a burst of hundreds of simulation steps once control returns.
constexpr double kMaxAccumulatedSeconds = 0.25;

#if defined(__unix__) || defined(__APPLE__)
class TemporaryMap {
public:
    TemporaryMap() : path_((std::filesystem::temp_directory_path() / "astroparty-arena-XXXXXX").string()) {
        const int fd = mkstemp(path_.data());
        if (fd == -1) {
            throw std::system_error(errno, std::generic_category(), "Could not create temporary arena file");
        }
        close(fd);
    }

    ~TemporaryMap() {
        std::error_code ignored;
        std::filesystem::remove(path_, ignored);
    }

    TemporaryMap(const TemporaryMap&) = delete;
    TemporaryMap& operator=(const TemporaryMap&) = delete;
    [[nodiscard]] const std::string& path() const { return path_; }

private:
    std::string path_;
};
#endif

core::MapDefinition generateMap(const ApplicationOptions& options, const core::SimulationConfig& config) {
#if defined(__unix__) || defined(__APPLE__)
    const TemporaryMap map;
    std::vector<std::string> command{
        "python3", ASTROPARTY_ARENA_GENERATOR, "--output", map.path(),
        "--ship-radius", std::to_string(config.shipCollisionRadius)
    };
    if (options.mapSeed) {
        command.push_back("--seed=" + *options.mapSeed);
    }
    if (options.mapSize) {
        command.push_back("--size=" + *options.mapSize);
    }
    std::vector<char*> argv;
    for (auto& argument : command) {
        argv.push_back(argument.data());
    }
    argv.push_back(nullptr);
    pid_t child;
    const int error = posix_spawnp(&child, argv.front(), nullptr, nullptr, argv.data(), environ);
    if (error != 0) {
        throw std::system_error(error, std::generic_category(), "Could not start python3 for arena generation");
    }
    int status;
    while (waitpid(child, &status, 0) == -1) {
        if (errno != EINTR) {
            throw std::system_error(errno, std::generic_category(), "Could not wait for arena generator");
        }
    }
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        throw std::runtime_error("Arena generation failed");
    }
    return core::MapDefinition::loadFromFile(map.path());
#else
    (void)options;
    (void)config;
    throw std::runtime_error("Automatic arena generation requires macOS or Linux");
#endif
}

// Classic Deathmatch keeps Milestone 1's exact built-in two-corner map;
// Team Resource Battle loads the config/map the caller selected (or their
// checked-in defaults -- see ApplicationOptions).
core::World buildWorld(const ApplicationOptions& options) {
    if (options.mode == core::GameMode::ClassicDeathmatch) {
        return core::World();
    }
    const core::SimulationConfig config = core::SimulationConfig::loadFromFile(options.configPath);
    const core::MapDefinition map = options.generateMap ? generateMap(options, config)
                                                        : core::MapDefinition::loadFromFile(options.mapPath);
    return core::World(config, map);
}

}  // namespace

Application::Application(ApplicationOptions options)
    : world_(buildWorld(options)),
      sdlLibrary_(SDL_INIT_VIDEO | SDL_INIT_EVENTS),
      window_("AstroParty-AI", kWindowWidth, kWindowHeight),
      renderer_(window_),
      gameRenderer_(renderer_),
      captureFrame_(std::move(options.captureFrame)) {
    const auto [width, height] = world_.arenaSize();
    const auto logicalSize = [](float dimension) {
        const double pixels = std::ceil(static_cast<double>(dimension));
        if (!(pixels > 0.0 && pixels <= std::numeric_limits<int>::max())) {
            throw std::runtime_error("Arena dimensions are outside the renderer's supported range");
        }
        return static_cast<int>(pixels);
    };
    if (SDL_RenderSetLogicalSize(renderer_.handle(), logicalSize(width), logicalSize(height)) != 0) {
        throw std::runtime_error(std::string("Could not fit arena to the window: ") + SDL_GetError());
    }
    if (options.mode == core::GameMode::ClassicDeathmatch) {
        // Player 1 uses the left-hand WASD cluster; Player 2 uses the arrow
        // keys, so both players have a disjoint set of keys on the same
        // keyboard. Upstream read mouse clicks for a single local player
        // (see docs/UPSTREAM_AUDIT.md, section 8); two simultaneous local
        // players need two independent input sources instead.
        shipControllers_.push_back(std::make_unique<input::KeyboardShipController>(
            SDL_SCANCODE_A, SDL_SCANCODE_D, SDL_SCANCODE_W, SDL_SCANCODE_S, SDL_SCANCODE_SPACE));
        shipControllers_.push_back(std::make_unique<input::KeyboardShipController>(
            SDL_SCANCODE_LEFT, SDL_SCANCODE_RIGHT, SDL_SCANCODE_UP, SDL_SCANCODE_DOWN, SDL_SCANCODE_SLASH));
    } else {
        // Team Resource Battle has no human controller in this milestone:
        // the window observes a 3v3 AI-vs-AI match, wired up exactly like
        // the headless runner (see ai::makeTeamControllers).
        shipControllers_ = ai::makeTeamControllers(world_, options.team0Strategy, options.team1Strategy);
    }
}

int Application::run() {
    const double timestep = 1.0 / world_.config().tickRateHz;
    auto previousTime = std::chrono::steady_clock::now();
    double accumulatedSeconds = 0.0;
    std::vector<ai::ShipRole> shipRoles;

    while (isRunning_) {
        processSdlEvents();

        const auto currentTime = std::chrono::steady_clock::now();
        accumulatedSeconds += std::chrono::duration<double>(currentTime - previousTime).count();
        previousTime = currentTime;
        accumulatedSeconds = std::min(accumulatedSeconds, std::max(timestep, kMaxAccumulatedSeconds));

        applyRestartCommandIfRequested();

        while (accumulatedSeconds >= timestep) {
            world_.step(collectShipActions(), world_.config().fixedDeltaSeconds());
            accumulatedSeconds -= timestep;
        }

        updateWindowTitleForMatchState();
        shipRoles.clear();
        if (world_.mode() == core::GameMode::TeamResourceBattle) {
            for (std::size_t i = 0; i < shipControllers_.size(); ++i) {
                const auto* controller = dynamic_cast<const ai::TeamShipController*>(shipControllers_[i].get());
                shipRoles.push_back(controller ? controller->lastRole() : ai::ShipRole::Defender);
            }
        }
        if (captureFrame_) {
            if (world_.teamMatchState().hasEnded()) {
                throw std::runtime_error("Match ended before a scoring frame could be captured");
            }
            const auto& scores = world_.teamScores();
            if (scores[0] + scores[1] > 0) {
                gameRenderer_.renderFrame(world_, captureFrame_->c_str(), shipRoles);
                std::cout << "Captured tick " << world_.elapsedTicks()
                          << " at " << scores[0] << ':' << scores[1] << '\n';
                return 0;
            }
        }
        gameRenderer_.renderFrame(world_, nullptr, shipRoles);
    }

    if (captureFrame_) {
        throw std::runtime_error("Match was closed before a frame could be captured");
    }
    return 0;
}

void Application::processSdlEvents() {
    SDL_Event event;
    while (SDL_PollEvent(&event) != 0) {
        if (event.type == SDL_QUIT) {
            isRunning_ = false;
        } else if (event.type == SDL_KEYDOWN && event.key.keysym.scancode == SDL_SCANCODE_ESCAPE) {
            isRunning_ = false;
        }
    }
}

void Application::applyRestartCommandIfRequested() {
    const Uint8* keyboardState = SDL_GetKeyboardState(nullptr);
    const bool restartKeyIsPressed = keyboardState[SDL_SCANCODE_R] != 0;
    if (restartKeyIsPressed && !restartKeyWasPressed_) {
        world_.reset();
    }
    restartKeyWasPressed_ = restartKeyIsPressed;
}

std::vector<core::ShipAction> Application::collectShipActions() const {
    std::vector<core::ShipAction> actions;
    actions.reserve(shipControllers_.size());
    for (std::size_t i = 0; i < shipControllers_.size(); ++i) {
        actions.push_back(shipControllers_[i]->decide(world_, static_cast<int>(i)));
    }
    return actions;
}

void Application::updateWindowTitleForMatchState() {
    if (world_.mode() == core::GameMode::ClassicDeathmatch) {
        updateWindowTitleForClassicDeathmatch();
    } else {
        updateWindowTitleForTeamResourceBattle();
    }
}

void Application::updateWindowTitleForClassicDeathmatch() {
    const core::MatchOutcome outcome = world_.matchState().outcome();
    if (outcome == lastAnnouncedOutcome_) {
        return;
    }
    lastAnnouncedOutcome_ = outcome;

    std::string title = "AstroParty-AI";
    switch (outcome) {
        case core::MatchOutcome::Undecided:
            title += " - P1: WASD + Space | P2: Arrows + Slash | R: restart";
            break;
        case core::MatchOutcome::ShipWon: {
            const int winningShipNumber = *world_.matchState().winningShipId() + 1;
            title += " - Player " + std::to_string(winningShipNumber) + " wins! Press R to restart";
            break;
        }
        case core::MatchOutcome::Draw:
            title += " - Draw! Press R to restart";
            break;
    }
    SDL_SetWindowTitle(window_.handle(), title.c_str());
}

void Application::updateWindowTitleForTeamResourceBattle() {
    const core::TeamMatchOutcome outcome = world_.teamMatchState().outcome();
    const std::array<int, 2>& scores = world_.teamScores();

    // Unlike Classic Deathmatch, the score itself changes well before the
    // match ends, so the title keeps refreshing every tick rather than only
    // on outcome changes -- there is no single-frame flicker risk here since
    // SDL_SetWindowTitle is cheap and this already runs at a fixed 60 Hz.
    std::string title =
        "AstroParty-AI - Team Resource Battle - " + std::to_string(scores[0]) + " : " + std::to_string(scores[1]) +
        " (first to " + std::to_string(world_.config().winningScore) + ")";
    switch (outcome) {
        case core::TeamMatchOutcome::Undecided:
            title += " | Orange: intercept | Purple: escort | White: self-defense | R: restart";
            break;
        case core::TeamMatchOutcome::TeamWon:
            title += " - Team " + std::to_string(*world_.teamMatchState().winningTeamId() + 1) +
                     " wins! Press R to restart";
            break;
        case core::TeamMatchOutcome::Draw:
            title += " - Draw! Press R to restart";
            break;
    }
    SDL_SetWindowTitle(window_.handle(), title.c_str());
}

}  // namespace astroparty
