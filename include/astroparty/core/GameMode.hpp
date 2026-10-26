#pragma once

namespace astroparty::core {

// Which rule set a World runs. Both modes share the exact same ship/bullet
// physics, obstacle collision, and arena bounds; they differ only in the
// win condition and in what happens when a ship is eliminated (see
// docs/ARCHITECTURE.md's "Game modes" section for the full comparison).
enum class GameMode {
    // The original two-player "last ship alive wins" mode: no teams, no
    // resources, no respawning. Evaluated via MatchState.
    ClassicDeathmatch,

    // Two teams of three ships compete to deliver Energy Cores to their own
    // home region. Eliminated ships respawn after a delay. Evaluated via
    // TeamMatchState.
    TeamResourceBattle,
};

}  // namespace astroparty::core
