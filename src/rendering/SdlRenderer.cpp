#include "astroparty/rendering/SdlRenderer.hpp"

#include <stdexcept>
#include <string>
#include <utility>

namespace astroparty::rendering {

SdlRenderer::SdlRenderer(SdlWindow& window)
    : renderer_(SDL_CreateRenderer(window.handle(), -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC)) {
    if (renderer_ == nullptr) {
        throw std::runtime_error(std::string("SDL_CreateRenderer failed: ") + SDL_GetError());
    }
}

SdlRenderer::~SdlRenderer() {
    if (renderer_ != nullptr) {
        SDL_DestroyRenderer(renderer_);
    }
}

SdlRenderer::SdlRenderer(SdlRenderer&& other) noexcept : renderer_(std::exchange(other.renderer_, nullptr)) {}

SdlRenderer& SdlRenderer::operator=(SdlRenderer&& other) noexcept {
    if (this != &other) {
        if (renderer_ != nullptr) {
            SDL_DestroyRenderer(renderer_);
        }
        renderer_ = std::exchange(other.renderer_, nullptr);
    }
    return *this;
}

}  // namespace astroparty::rendering
