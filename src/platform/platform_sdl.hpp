#pragma once

// See src/render/renderer.hpp: mingw_compat.hpp's empty SAL macros collide
// with libstdc++ parameter names, so drop them before any STL headers.
#ifdef __in
#undef __in
#endif
#ifdef __out
#undef __out
#endif
#ifdef __inout
#undef __inout
#endif

#include <cstdint>
#include <SDL2/SDL.h>

namespace th06 {

struct WindowConfig {
    int width = 640;
    int height = 480;
    const char* title = "Touhou Koumakyou ~ the Embodiment of Scarlet Devil";
    bool fullscreen = false;
};

class PlatformWindow {
public:
    static PlatformWindow& Instance();

    bool Initialize(const WindowConfig& config);
    void Shutdown();
    bool PollEvents();

    SDL_Window* GetWindow() const { return window_; }
    void GetWindowSize(int* w, int* h) const;
    void GetDrawableSize(int* w, int* h) const;
    bool IsFullscreen() const { return fullscreen_; }
    bool ShouldQuit() const { return shouldQuit_; }

    uint16_t ReadInput();

private:
    PlatformWindow() = default;
    ~PlatformWindow() = default;

    SDL_Window* window_ = nullptr;
    SDL_GameController* controller_ = nullptr;
    bool fullscreen_ = false;
    bool shouldQuit_ = false;
};

} // namespace th06
