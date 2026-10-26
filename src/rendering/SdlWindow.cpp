#include "astroparty/rendering/SdlWindow.hpp"

#include <stdexcept>
#include <utility>

namespace astroparty::rendering {

SdlWindow::SdlWindow(const std::string& title, int width, int height)
    : window_(SDL_CreateWindow(title.c_str(), SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, width, height,
                                SDL_WINDOW_SHOWN)) {
    if (window_ == nullptr) {
        throw std::runtime_error(std::string("SDL_CreateWindow failed: ") + SDL_GetError());
    }
}

SdlWindow::~SdlWindow() {
    if (window_ != nullptr) {
        SDL_DestroyWindow(window_);
    }
}

SdlWindow::SdlWindow(SdlWindow&& other) noexcept : window_(std::exchange(other.window_, nullptr)) {}

SdlWindow& SdlWindow::operator=(SdlWindow&& other) noexcept {
    if (this != &other) {
        if (window_ != nullptr) {
            SDL_DestroyWindow(window_);
        }
        window_ = std::exchange(other.window_, nullptr);
    }
    return *this;
}

}  // namespace astroparty::rendering
