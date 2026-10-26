#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "astroparty/core/GameMode.hpp"
#include "astroparty/core/MatchState.hpp"
#include "astroparty/core/TeamMatchState.hpp"
#include "astroparty/core/World.hpp"
#include "astroparty/input/ShipController.hpp"
#include "astroparty/rendering/GameRenderer.hpp"
#include "astroparty/rendering/SdlLibraryGuard.hpp"
#include "astroparty/rendering/SdlRenderer.hpp"
#include "astroparty/rendering/SdlWindow.hpp"

namespace astroparty {

// Selects what Application displays: two human players in Classic
// Deathmatch (the default, unchanged from Milestone 1), or an observed 3v3
// Team Resource Battle match between AI-controlled teams. There is no menu
// system -- this is the "simple CLI" selection the Team Resource Battle
// deliverable asks for; see main.cpp's command-line parsing.
struct ApplicationOptions {
    core::GameMode mode = core::GameMode::ClassicDeathmatch;

    // Only consulted when mode == TeamResourceBattle. Defaulted to the
    // checked-in team config/map so `--mode team` alone works when run from
    // the repository root, the same convention the headless executable
    // already relies on for its own config/map flags.
    std::string configPath = "configs/team_resource_battle.json";
    std::string mapPath = "configs/maps/team_arena.json";
    std::string team0Strategy = "strategic";
    std::string team1Strategy = "baseline";
};

// Owns SDL's lifetime, the window, the simulation, and the per-ship
// controllers, and runs the fixed-timestep main loop that ties them
// together.
//
// Application is deliberately the only class in the project that knows
// about all of simulation, rendering, input, and SDL setup at once -- every
// other class is scoped to exactly one of those concerns (see
// docs/ARCHITECTURE.md). This mirrors the Phase 2 instruction to keep
// rendering, input, and simulation separated rather than repeating
// upstream's coupling. Both game modes share this exact same loop --
// construction is the only place they differ (see Application.cpp).
class Application {
public:
    explicit Application(ApplicationOptions options = {});

    // Runs the main loop until the window is closed or Escape is pressed.
    // Returns a process exit code suitable for returning from main().
    int run();

private:
    rendering::SdlLibraryGuard sdlLibrary_;
    rendering::SdlWindow window_;
    rendering::SdlRenderer renderer_;
    rendering::GameRenderer gameRenderer_;
    core::World world_;
    std::vector<std::unique_ptr<input::IShipController>> shipControllers_;

    bool isRunning_ = true;
    bool restartKeyWasPressed_ = false;
    core::MatchOutcome lastAnnouncedOutcome_ = core::MatchOutcome::Undecided;

    void processSdlEvents();
    void applyRestartCommandIfRequested();
    [[nodiscard]] std::vector<core::ShipAction> collectShipActions() const;

    // Reports match state through the window title rather than on-screen
    // text, so Milestone 1 needs no font-rendering dependency (SDL_ttf) --
    // see the Phase 2 instruction to avoid spending time on rendering
    // infrastructure beyond what gameplay needs. Dispatches on world_.mode()
    // to the right match-state type.
    void updateWindowTitleForMatchState();
    void updateWindowTitleForClassicDeathmatch();
    void updateWindowTitleForTeamResourceBattle();
};

}  // namespace astroparty
