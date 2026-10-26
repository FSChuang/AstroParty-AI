#pragma once

#include <SDL2/SDL.h>

#include "astroparty/rendering/SdlWindow.hpp"

namespace astroparty::rendering {

// RAII wrapper around an SDL_Renderer bound to an SdlWindow.
//
// Construction throws std::runtime_error (with SDL_GetError() detail) on
// failure; destruction destroys the renderer exactly once, including after a
// move.
class SdlRenderer {
public:
    explicit SdlRenderer(SdlWindow& window);
    ~SdlRenderer();

    SdlRenderer(const SdlRenderer&) = delete;
    SdlRenderer& operator=(const SdlRenderer&) = delete;
    SdlRenderer(SdlRenderer&& other) noexcept;
    SdlRenderer& operator=(SdlRenderer&& other) noexcept;

    [[nodiscard]] SDL_Renderer* handle() const noexcept { return renderer_; }

private:
    SDL_Renderer* renderer_;
};

}  // namespace astroparty::rendering
