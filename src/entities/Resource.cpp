#include "astroparty/entities/Resource.hpp"

namespace astroparty::entities {

Resource::Resource(int id, core::Vector2 spawnPosition) noexcept : id_(id), spawnPosition_(spawnPosition) {}

void Resource::pickUp(int shipId) noexcept {
    state_ = ResourceState::Carried;
    carrierShipId_ = shipId;
}

void Resource::returnToSpawn() noexcept {
    state_ = ResourceState::AtSpawn;
    carrierShipId_.reset();
}

}  // namespace astroparty::entities
