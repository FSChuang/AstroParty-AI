#include "Application.hpp"

#include <SDL2/SDL.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <string>

#include "astroparty/ai/TeamControllerFactory.hpp"
#include "astroparty/core/MapDefinition.hpp"
#include "astroparty/core/SimulationConfig.hpp"
#include "astroparty/input/KeyboardShipController.hpp"

namespace astroparty {

namespace {

constexpr int kWindowWidth = 1280;
constexpr int kWindowHeight = 720;

// World always steps with this fixed delta time, regardless of how fast the
// display can render -- see docs/ARCHITECTURE.md for why Milestone 1 uses a
// fixed-timestep accumulator rather than a variable-rate update.
constexpr double kFixedTimestepSeconds = 1.0 / 60.0;

// Caps how much simulation time a single frame can catch up on, so a long
// stall (e.g. the user drags the window, or a debugger pauses the process)
// cannot force a burst of hundreds of simulation steps once control returns.
constexpr double kMaxAccumulatedSeconds = 0.25;

// Classic Deathmatch keeps Milestone 1's exact built-in two-corner map;
// Team Resource Battle loads the config/map the caller selected (or their
// checked-in defaults -- see ApplicationOptions).
core::World buildWorld(const ApplicationOptions& options) {
    if (options.mode == core::GameMode::ClassicDeathmatch) {
        return core::World();
    }
    const core::SimulationConfig config = core::SimulationConfig::loadFromFile(options.configPath);
    const core::MapDefinition map = core::MapDefinition::loadFromFile(options.mapPath);
    return core::World(config, map);
}

}  // namespace

Application::Application(ApplicationOptions options)
    : sdlLibrary_(SDL_INIT_VIDEO | SDL_INIT_EVENTS),
      window_("AstroParty-AI", kWindowWidth, kWindowHeight),
      renderer_(window_),
      gameRenderer_(renderer_),
      world_(buildWorld(options)) {
    if (options.mode == core::GameMode::ClassicDeathmatch) {
        // Player 1 uses the left-hand WASD cluster; Player 2 uses the arrow
        // keys, so both players have a disjoint set of keys on the same
        // keyboard. Upstream read mouse clicks for a single local player
        // (see docs/UPSTREAM_AUDIT.md, section 8); two simultaneous local
        // players need two independent input sources instead.
        shipControllers_.push_back(std::make_unique<input::KeyboardShipController>(SDL_SCANCODE_D, SDL_SCANCODE_W));
        shipControllers_.push_back(
            std::make_unique<input::KeyboardShipController>(SDL_SCANCODE_RIGHT, SDL_SCANCODE_UP));
    } else {
        // Team Resource Battle has no human controller in this milestone:
        // the window observes a 3v3 AI-vs-AI match, wired up exactly like
        // the headless runner (see ai::makeTeamControllers).
        shipControllers_ = ai::makeTeamControllers(world_, options.team0Strategy, options.team1Strategy);
    }
}

int Application::run() {
    auto previousTime = std::chrono::steady_clock::now();
    double accumulatedSeconds = 0.0;

    while (isRunning_) {
        processSdlEvents();

        const auto currentTime = std::chrono::steady_clock::now();
        accumulatedSeconds += std::chrono::duration<double>(currentTime - previousTime).count();
        previousTime = currentTime;
        accumulatedSeconds = std::min(accumulatedSeconds, kMaxAccumulatedSeconds);

        applyRestartCommandIfRequested();

        while (accumulatedSeconds >= kFixedTimestepSeconds) {
            world_.step(collectShipActions(), static_cast<float>(kFixedTimestepSeconds));
            accumulatedSeconds -= kFixedTimestepSeconds;
        }

        updateWindowTitleForMatchState();
        gameRenderer_.renderFrame(world_);
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
            title += " - P1: D/W  P2: Arrow Right/Up  (R to restart)";
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
        "AstroParty-AI - Team Resource Battle - " + std::to_string(scores[0]) + " : " + std::to_string(scores[1]);
    switch (outcome) {
        case core::TeamMatchOutcome::Undecided:
            title += " (observing AI vs AI, R to restart)";
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
