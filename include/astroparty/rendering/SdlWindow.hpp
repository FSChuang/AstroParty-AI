#pragma once

#include <SDL2/SDL.h>

#include <string>

namespace astroparty::rendering {

// RAII wrapper around an SDL_Window.
//
// Construction creates the OS window immediately and throws
// std::runtime_error (with SDL_GetError() detail) on failure; destruction
// destroys the window exactly once, including after a move.
class SdlWindow {
public:
    SdlWindow(const std::string& title, int width, int height);
    ~SdlWindow();

    SdlWindow(const SdlWindow&) = delete;
    SdlWindow& operator=(const SdlWindow&) = delete;
    SdlWindow(SdlWindow&& other) noexcept;
    SdlWindow& operator=(SdlWindow&& other) noexcept;

    [[nodiscard]] SDL_Window* handle() const noexcept { return window_; }

private:
    SDL_Window* window_;
};

}  // namespace astroparty::rendering
