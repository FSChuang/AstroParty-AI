#pragma once

#include <cstdint>

namespace astroparty::rendering {

// RAII handle for SDL's global library lifetime.
//
// Construction calls SDL_Init(flags) and throws std::runtime_error (with
// SDL_GetError() detail) if it fails; destruction always calls SDL_Quit()
// exactly once. Exactly one instance should exist for the process's
// lifetime, so copying and moving are both disabled.
class SdlLibraryGuard {
public:
    explicit SdlLibraryGuard(std::uint32_t initFlags);
    ~SdlLibraryGuard();

    SdlLibraryGuard(const SdlLibraryGuard&) = delete;
    SdlLibraryGuard& operator=(const SdlLibraryGuard&) = delete;
    SdlLibraryGuard(SdlLibraryGuard&&) = delete;
    SdlLibraryGuard& operator=(SdlLibraryGuard&&) = delete;
};

}  // namespace astroparty::rendering
