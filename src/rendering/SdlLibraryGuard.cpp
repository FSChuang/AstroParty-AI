#include "astroparty/rendering/SdlLibraryGuard.hpp"

#include <SDL2/SDL.h>

#include <stdexcept>
#include <string>

namespace astroparty::rendering {

SdlLibraryGuard::SdlLibraryGuard(std::uint32_t initFlags) {
    if (SDL_Init(initFlags) != 0) {
        throw std::runtime_error(std::string("SDL_Init failed: ") + SDL_GetError());
    }
}

SdlLibraryGuard::~SdlLibraryGuard() { SDL_Quit(); }

}  // namespace astroparty::rendering
