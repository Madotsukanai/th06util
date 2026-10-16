#include "platform_sdl.hpp"
#include <SDL2/SDL_vulkan.h>
#include <cstdio>

namespace th06 {

// Touhou button bitmasks (from Controller.hpp)
constexpr uint16_t BTN_SHOOT      = 0x0001;
constexpr uint16_t BTN_BOMB       = 0x0002;
constexpr uint16_t BTN_FOCUS      = 0x0004;
constexpr uint16_t BTN_MENU       = 0x0008;
constexpr uint16_t BTN_UP         = 0x0010;
constexpr uint16_t BTN_DOWN       = 0x0020;
constexpr uint16_t BTN_LEFT       = 0x0040;
constexpr uint16_t BTN_RIGHT      = 0x0080;
constexpr uint16_t BTN_SKIP       = 0x0100;
constexpr uint16_t BTN_Q          = 0x0200;
constexpr uint16_t BTN_S          = 0x0400;
constexpr uint16_t BTN_HOME       = 0x0800;
constexpr uint16_t BTN_ENTER      = 0x1000;

PlatformWindow& PlatformWindow::Instance() {
    static PlatformWindow instance;
    return instance;
}

bool PlatformWindow::Initialize(const WindowConfig& config) {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER | SDL_INIT_AUDIO) != 0) {
        fprintf(stderr, "[Platform] SDL_Init failed: %s\n", SDL_GetError());
        return false;
    }

    uint32_t flags = SDL_WINDOW_VULKAN | SDL_WINDOW_SHOWN | SDL_WINDOW_ALLOW_HIGHDPI;
    if (config.fullscreen) {
        flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
    }

    window_ = SDL_CreateWindow(
        config.title,
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        config.width,
        config.height,
        flags
    );

    if (!window_) {
        fprintf(stderr, "[Platform] SDL_CreateWindow failed: %s\n", SDL_GetError());
        return false;
    }

    fullscreen_ = config.fullscreen;
    shouldQuit_ = false;

    // Open first game controller if available
    for (int i = 0; i < SDL_NumJoysticks(); i++) {
        if (SDL_IsGameController(i)) {
            controller_ = SDL_GameControllerOpen(i);
            if (controller_) {
                fprintf(stdout, "[Platform] Opened GameController: %s\n", SDL_GameControllerName(controller_));
                break;
            }
        }
    }

    return true;
}

void PlatformWindow::Shutdown() {
    if (controller_) {
        SDL_GameControllerClose(controller_);
        controller_ = nullptr;
    }
    if (window_) {
        SDL_DestroyWindow(window_);
        window_ = nullptr;
    }
    SDL_Quit();
}

bool PlatformWindow::PollEvents() {
    SDL_Event ev;
    while (SDL_PollEvent(&ev)) {
        if (ev.type == SDL_QUIT) {
            shouldQuit_ = true;
            return false;
        }
        if (ev.type == SDL_CONTROLLERDEVICEADDED) {
            if (!controller_) {
                controller_ = SDL_GameControllerOpen(ev.cdevice.which);
            }
        } else if (ev.type == SDL_CONTROLLERDEVICEREMOVED) {
            if (controller_ && ev.cdevice.which == SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(controller_))) {
                SDL_GameControllerClose(controller_);
                controller_ = nullptr;
            }
        }
    }
    return !shouldQuit_;
}

void PlatformWindow::GetWindowSize(int* w, int* h) const {
    if (window_) {
        SDL_GetWindowSize(window_, w, h);
    } else {
        if (w) *w = 640;
        if (h) *h = 480;
    }
}

void PlatformWindow::GetDrawableSize(int* w, int* h) const {
    if (window_) {
        SDL_Vulkan_GetDrawableSize(window_, w, h);
    } else {
        if (w) *w = 640;
        if (h) *h = 480;
    }
}

uint16_t PlatformWindow::ReadInput() {
    uint16_t buttons = 0;
    const uint8_t* keys = SDL_GetKeyboardState(nullptr);

    if (keys[SDL_SCANCODE_UP] || keys[SDL_SCANCODE_W] || keys[SDL_SCANCODE_KP_8])       buttons |= BTN_UP;
    if (keys[SDL_SCANCODE_DOWN] || keys[SDL_SCANCODE_S] || keys[SDL_SCANCODE_KP_2])   buttons |= BTN_DOWN;
    if (keys[SDL_SCANCODE_LEFT] || keys[SDL_SCANCODE_A] || keys[SDL_SCANCODE_KP_4])   buttons |= BTN_LEFT;
    if (keys[SDL_SCANCODE_RIGHT] || keys[SDL_SCANCODE_D] || keys[SDL_SCANCODE_KP_6])  buttons |= BTN_RIGHT;
    if (keys[SDL_SCANCODE_Z])                                                          buttons |= BTN_SHOOT;
    if (keys[SDL_SCANCODE_RETURN] || keys[SDL_SCANCODE_KP_ENTER])                      buttons |= BTN_ENTER | BTN_SHOOT;
    if (keys[SDL_SCANCODE_X] || keys[SDL_SCANCODE_BACKSPACE])                          buttons |= BTN_BOMB;
    if (keys[SDL_SCANCODE_LSHIFT] || keys[SDL_SCANCODE_RSHIFT])                        buttons |= BTN_FOCUS;
    if (keys[SDL_SCANCODE_ESCAPE])                                                     buttons |= BTN_MENU;
    if (keys[SDL_SCANCODE_LCTRL] || keys[SDL_SCANCODE_RCTRL])                           buttons |= BTN_SKIP;
    if (keys[SDL_SCANCODE_Q])                                                          buttons |= BTN_Q;
    if (keys[SDL_SCANCODE_HOME] || keys[SDL_SCANCODE_R])                                buttons |= BTN_HOME;

    // Gamepad input
    if (controller_) {
        if (SDL_GameControllerGetButton(controller_, SDL_CONTROLLER_BUTTON_DPAD_UP))    buttons |= BTN_UP;
        if (SDL_GameControllerGetButton(controller_, SDL_CONTROLLER_BUTTON_DPAD_DOWN))  buttons |= BTN_DOWN;
        if (SDL_GameControllerGetButton(controller_, SDL_CONTROLLER_BUTTON_DPAD_LEFT))  buttons |= BTN_LEFT;
        if (SDL_GameControllerGetButton(controller_, SDL_CONTROLLER_BUTTON_DPAD_RIGHT)) buttons |= BTN_RIGHT;

        int16_t lx = SDL_GameControllerGetAxis(controller_, SDL_CONTROLLER_AXIS_LEFTX);
        int16_t ly = SDL_GameControllerGetAxis(controller_, SDL_CONTROLLER_AXIS_LEFTY);
        constexpr int16_t DEADZONE = 10000;
        if (lx < -DEADZONE) buttons |= BTN_LEFT;
        if (lx >  DEADZONE) buttons |= BTN_RIGHT;
        if (ly < -DEADZONE) buttons |= BTN_UP;
        if (ly >  DEADZONE) buttons |= BTN_DOWN;

        if (SDL_GameControllerGetButton(controller_, SDL_CONTROLLER_BUTTON_A))             buttons |= BTN_SHOOT | BTN_ENTER;
        if (SDL_GameControllerGetButton(controller_, SDL_CONTROLLER_BUTTON_B))             buttons |= BTN_BOMB | BTN_MENU;
        if (SDL_GameControllerGetButton(controller_, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER) ||
            SDL_GameControllerGetAxis(controller_, SDL_CONTROLLER_AXIS_TRIGGERRIGHT) > 10000) buttons |= BTN_FOCUS;
        if (SDL_GameControllerGetButton(controller_, SDL_CONTROLLER_BUTTON_START))         buttons |= BTN_MENU;
    }

    return buttons;
}

} // namespace th06
