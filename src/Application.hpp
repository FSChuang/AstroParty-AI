#pragma once

#include <memory>
#include <vector>

#include "astroparty/core/MatchState.hpp"
#include "astroparty/core/World.hpp"
#include "astroparty/input/ShipController.hpp"
#include "astroparty/rendering/GameRenderer.hpp"
#include "astroparty/rendering/SdlLibraryGuard.hpp"
#include "astroparty/rendering/SdlRenderer.hpp"
#include "astroparty/rendering/SdlWindow.hpp"

namespace astroparty {

// Owns SDL's lifetime, the window, the simulation, and the per-player
// controllers, and runs the fixed-timestep main loop that ties them
// together.
//
// Application is deliberately the only class in the project that knows
// about all of simulation, rendering, input, and SDL setup at once -- every
// other class is scoped to exactly one of those concerns (see
// docs/ARCHITECTURE.md). This mirrors the Phase 2 instruction to keep
// rendering, input, and simulation separated rather than repeating
// upstream's coupling.
class Application {
public:
    Application();

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
    // infrastructure beyond what gameplay needs.
    void updateWindowTitleForMatchState();
};

}  // namespace astroparty
