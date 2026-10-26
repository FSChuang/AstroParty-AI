#pragma once

#include <optional>

#include "astroparty/core/Vector2.hpp"

namespace astroparty::entities {

enum class ResourceState { AtSpawn, Carried };

// An Energy Core for Team Resource Battle: a team-neutral collectible that
// is either sitting at its fixed spawn point or being carried by a ship.
//
// A Resource has only two possible positions: its own spawnPosition() when
// AtSpawn, or wherever its carrying ship currently is when Carried -- there
// is no third, independent "dropped at some arbitrary point" state. See
// docs/ARCHITECTURE.md's "Resource rules" section for why this was chosen:
// it keeps a resource's position always derivable from state everyone
// already tracks, rather than introducing a second, independent source of
// truth for position that could drift out of sync.
class Resource {
public:
    Resource(int id, core::Vector2 spawnPosition) noexcept;

    [[nodiscard]] int id() const noexcept { return id_; }
    [[nodiscard]] core::Vector2 spawnPosition() const noexcept { return spawnPosition_; }
    [[nodiscard]] ResourceState state() const noexcept { return state_; }

    // Valid only when state() == ResourceState::Carried.
    [[nodiscard]] std::optional<int> carrierShipId() const noexcept { return carrierShipId_; }

    // Marks this resource as picked up by the given ship.
    void pickUp(int shipId) noexcept;

    // Returns this resource to its spawn point and clears its carrier.
    // Used both for a successful delivery (the core becomes available
    // again immediately) and for a dropped core (its carrier was
    // eliminated while carrying it).
    void returnToSpawn() noexcept;

private:
    int id_;
    core::Vector2 spawnPosition_;
    ResourceState state_ = ResourceState::AtSpawn;
    std::optional<int> carrierShipId_;
};

}  // namespace astroparty::entities
