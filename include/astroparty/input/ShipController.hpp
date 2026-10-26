#pragma once

#include "astroparty/core/Action.hpp"

namespace astroparty::input {

// Produces a ShipAction once per simulation step.
//
// Implemented today by KeyboardShipController and, in a later milestone, by
// an AI controller -- World only ever sees the resulting ShipAction and
// never needs to know which kind of controller produced it. This is the
// seam that lets AstroParty-AI add AI-controlled ships later without
// changing any simulation code (astro-party-workspace/audit/06_implementation_roadmap.md,
// Milestone 2/3).
class IShipController {
public:
    virtual ~IShipController() = default;

    [[nodiscard]] virtual core::ShipAction pollAction() const = 0;
};

}  // namespace astroparty::input
