#include "Controller.hpp"

#include "GameErrorContext.hpp"
#include "Supervisor.hpp"
#include "i18n.hpp"
#include "utils.hpp"

namespace th06
{
JOYCAPSA g_JoystickCaps;
u16 g_FocusButtonConflictState;
KeyConfig g_KeyConfig;

u16 Controller::GetJoystickCaps(void)
{
    JOYINFOEX pji;

    pji.dwSize = sizeof(JOYINFOEX);
    pji.dwFlags = JOY_RETURNALL;

    if (joyGetPosEx(0, &pji) != MMSYSERR_NOERROR)
    {
        g_GameErrorContext.Log(TH_ERR_NO_PAD_FOUND);
        return 1;
    }

    joyGetDevCapsA(0, &g_JoystickCaps, sizeof(g_JoystickCaps));
    return 0;
}

#define JOYSTICK_MIDPOINT(min, max) ((min + max) / 2)
#define JOYSTICK_BUTTON_PRESSED(button, x, y) (x > y ? button : 0)
#define JOYSTICK_BUTTON_PRESSED_INVERT(button, x, y) (x < y ? button : 0)
#define KEYBOARD_KEY_PRESSED(button, x) keyboardState[x] & 0x80 ? button : 0

u16 Controller::GetControllerInput(u16 buttons)
{

    JOYINFOEX aa;
    u32 ab;
    u32 ac;
    DIJOYSTATE2 a0;
    u32 a2;
    HRESULT aaa;

    if (g_Supervisor.controller == NULL)
    {
        memset(&aa, 0, sizeof(aa));
        aa.dwSize = sizeof(JOYINFOEX);
        aa.dwFlags = JOY_RETURNALL;

        if (joyGetPosEx(0, &aa) != MMSYSERR_NOERROR)
        {
            return buttons;
        }

        ac = SetButtonFromControllerInputs(&buttons, g_Supervisor.cfg.controllerMapping.shootButton, TH_BUTTON_SHOOT,
                                           aa.dwButtons);

        if (g_ControllerMapping.shootButton != g_ControllerMapping.focusButton)
        {
            SetButtonFromControllerInputs(&buttons, g_Supervisor.cfg.controllerMapping.focusButton, TH_BUTTON_FOCUS,
                                          aa.dwButtons);
        }
        else
        {
            if (ac != 0)
            {
                if (g_FocusButtonConflictState < 16)
                {
                    g_FocusButtonConflictState++;
                }

                if (g_FocusButtonConflictState >= 8)
                {
                    buttons |= TH_BUTTON_FOCUS;
                }
            }
            else
            {
                if (g_FocusButtonConflictState > 8)
                {
                    g_FocusButtonConflictState -= 8;
                }
                else
                {
                    g_FocusButtonConflictState = 0;
                }
            }
        }

        SetButtonFromControllerInputs(&buttons, g_Supervisor.cfg.controllerMapping.bombButton, TH_BUTTON_BOMB,
                                      aa.dwButtons);
        SetButtonFromControllerInputs(&buttons, g_Supervisor.cfg.controllerMapping.menuButton, TH_BUTTON_MENU,
                                      aa.dwButtons);
        SetButtonFromControllerInputs(&buttons, g_Supervisor.cfg.controllerMapping.upButton, TH_BUTTON_UP,
                                      aa.dwButtons);
        SetButtonFromControllerInputs(&buttons, g_Supervisor.cfg.controllerMapping.downButton, TH_BUTTON_DOWN,
                                      aa.dwButtons);
        SetButtonFromControllerInputs(&buttons, g_Supervisor.cfg.controllerMapping.leftButton, TH_BUTTON_LEFT,
                                      aa.dwButtons);
        SetButtonFromControllerInputs(&buttons, g_Supervisor.cfg.controllerMapping.rightButton, TH_BUTTON_RIGHT,
                                      aa.dwButtons);
        SetButtonFromControllerInputs(&buttons, g_Supervisor.cfg.controllerMapping.skipButton, TH_BUTTON_SKIP,
                                      aa.dwButtons);

        ab = ((g_JoystickCaps.wXmax - g_JoystickCaps.wXmin) / 2 / 2);

        buttons |= JOYSTICK_BUTTON_PRESSED(TH_BUTTON_RIGHT, aa.dwXpos,
                                           JOYSTICK_MIDPOINT(g_JoystickCaps.wXmin, g_JoystickCaps.wXmax) + ab);
        buttons |= JOYSTICK_BUTTON_PRESSED(
            TH_BUTTON_LEFT, JOYSTICK_MIDPOINT(g_JoystickCaps.wXmin, g_JoystickCaps.wXmax) - ab, aa.dwXpos);

        ab = ((g_JoystickCaps.wYmax - g_JoystickCaps.wYmin) / 2 / 2);
        buttons |= JOYSTICK_BUTTON_PRESSED(TH_BUTTON_DOWN, aa.dwYpos,
                                           JOYSTICK_MIDPOINT(g_JoystickCaps.wYmin, g_JoystickCaps.wYmax) + ab);
        buttons |= JOYSTICK_BUTTON_PRESSED(
            TH_BUTTON_UP, JOYSTICK_MIDPOINT(g_JoystickCaps.wYmin, g_JoystickCaps.wYmax) - ab, aa.dwYpos);

        return buttons;
    }
    else
    {

        aaa = g_Supervisor.controller->Poll();
        if (FAILED(aaa))
        {
            i32 retryCount = 0;

            utils::DebugPrint2("error : DIERR_INPUTLOST\n");
            aaa = g_Supervisor.controller->Acquire();

            while (aaa == DIERR_INPUTLOST)
            {
                aaa = g_Supervisor.controller->Acquire();
                utils::DebugPrint2("error : DIERR_INPUTLOST %d\n", retryCount);

                retryCount++;

                if (retryCount >= 400)
                {
                    return buttons;
                }
            }

            return buttons;
        }
        else
        {
            memset(&a0, 0, sizeof(a0));

            aaa = g_Supervisor.controller->GetDeviceState(sizeof(a0), &a0);

            if (FAILED(aaa))
            {
                return buttons;
            }

            a2 = SetButtonFromDirectInputJoystate(&buttons, g_Supervisor.cfg.controllerMapping.shootButton,
                                                  TH_BUTTON_SHOOT, a0.rgbButtons);

            if (g_Supervisor.cfg.controllerMapping.shootButton != g_Supervisor.cfg.controllerMapping.focusButton)
            {
                SetButtonFromDirectInputJoystate(&buttons, g_Supervisor.cfg.controllerMapping.focusButton,
                                                 TH_BUTTON_FOCUS, a0.rgbButtons);
            }
            else
            {
                if (a2 != 0)
                {
                    if (g_FocusButtonConflictState < 16)
                    {
                        g_FocusButtonConflictState++;
                    }

                    if (g_FocusButtonConflictState >= 8)
                    {
                        buttons |= TH_BUTTON_FOCUS;
                    }
                }
                else
                {
                    if (g_FocusButtonConflictState > 8)
                    {
                        g_FocusButtonConflictState -= 8;
                    }
                    else
                    {
                        g_FocusButtonConflictState = 0;
                    }
                }
            }

            SetButtonFromDirectInputJoystate(&buttons, g_Supervisor.cfg.controllerMapping.bombButton, TH_BUTTON_BOMB,
                                             a0.rgbButtons);
            SetButtonFromDirectInputJoystate(&buttons, g_Supervisor.cfg.controllerMapping.menuButton, TH_BUTTON_MENU,
                                             a0.rgbButtons);
            SetButtonFromDirectInputJoystate(&buttons, g_Supervisor.cfg.controllerMapping.upButton, TH_BUTTON_UP,
                                             a0.rgbButtons);
            SetButtonFromDirectInputJoystate(&buttons, g_Supervisor.cfg.controllerMapping.downButton, TH_BUTTON_DOWN,
                                             a0.rgbButtons);
            SetButtonFromDirectInputJoystate(&buttons, g_Supervisor.cfg.controllerMapping.leftButton, TH_BUTTON_LEFT,
                                             a0.rgbButtons);
            SetButtonFromDirectInputJoystate(&buttons, g_Supervisor.cfg.controllerMapping.rightButton, TH_BUTTON_RIGHT,
                                             a0.rgbButtons);
            SetButtonFromDirectInputJoystate(&buttons, g_Supervisor.cfg.controllerMapping.skipButton, TH_BUTTON_SKIP,
                                             a0.rgbButtons);

            buttons |= JOYSTICK_BUTTON_PRESSED(TH_BUTTON_RIGHT, a0.lX, g_Supervisor.cfg.padXAxis);
            buttons |= JOYSTICK_BUTTON_PRESSED_INVERT(TH_BUTTON_LEFT, a0.lX, -g_Supervisor.cfg.padXAxis);
            buttons |= JOYSTICK_BUTTON_PRESSED(TH_BUTTON_DOWN, a0.lY, g_Supervisor.cfg.padYAxis);
            buttons |= JOYSTICK_BUTTON_PRESSED_INVERT(TH_BUTTON_UP, a0.lY, -g_Supervisor.cfg.padYAxis);
        }
    }

    return buttons;
}

u32 Controller::SetButtonFromDirectInputJoystate(u16 *outButtons, i16 controllerButtonToTest,
                                                 enum TouhouButton touhouButton, u8 *inputButtons)
{
    if (controllerButtonToTest < 0)
    {
        return 0;
    }

    *outButtons |= (inputButtons[controllerButtonToTest] & 0x80 ? touhouButton & 0xFFFF : 0);

    return inputButtons[controllerButtonToTest] & 0x80 ? touhouButton & 0xFFFF : 0;
}

u32 Controller::SetButtonFromControllerInputs(u16 *outButtons, i16 controllerButtonToTest,
                                              enum TouhouButton touhouButton, u32 inputButtons)
{
    DWORD mask;

    if (controllerButtonToTest < 0)
    {
        return 0;
    }

    mask = 1 << controllerButtonToTest;

    *outButtons |= (inputButtons & mask ? touhouButton & 0xFFFF : 0);

    return inputButtons & mask ? touhouButton & 0xFFFF : 0;
}

u8 g_ControllerData[(32 * 4)];

u8 *th06::Controller::GetControllerState()
{
    JOYINFOEX joyinfoex;
    u32 joyButtonBit;
    u32 joyButtonIndex;

    i32 dires;
    DIJOYSTATE2 dijoystate2;
    i32 diRetryCount;

    memset(&g_ControllerData, 0, sizeof(g_ControllerData));
    if (g_Supervisor.controller == NULL)
    {
        memset(&joyinfoex, 0, sizeof(JOYINFOEX));
        joyinfoex.dwSize = sizeof(JOYINFOEX);
        joyinfoex.dwFlags = JOY_RETURNALL;
        if (joyGetPosEx(0, &joyinfoex) != JOYERR_NOERROR)
        {
            return g_ControllerData;
        }
        for (joyButtonBit = joyinfoex.dwButtons, joyButtonIndex = 0; joyButtonIndex < 32;
             joyButtonIndex += 1, joyButtonBit >>= 1)
        {
            if ((joyButtonBit & 1) != 0)
            {
                g_ControllerData[joyButtonIndex] = 0x80;
            }
        }
        return g_ControllerData;
    }
    else
    {
        dires = g_Supervisor.controller->Poll();
        if (FAILED(dires))
        {
            diRetryCount = 0;
            utils::DebugPrint2("error : DIERR_INPUTLOST\n");
            dires = g_Supervisor.controller->Acquire();
            while (dires == DIERR_INPUTLOST)
            {
                dires = g_Supervisor.controller->Acquire();
                utils::DebugPrint2("error : DIERR_INPUTLOST %d\n", diRetryCount);
                diRetryCount++;
                if (diRetryCount >= 400)
                {
                    return g_ControllerData;
                }
            }
            return g_ControllerData;
        }
        /* */ g_Supervisor.controller->GetDeviceState(sizeof(DIJOYSTATE2), &dijoystate2);

        if (FAILED(dires))
        {
            return g_ControllerData;
        }
        memcpy(&g_ControllerData, dijoystate2.rgbButtons, sizeof(dijoystate2.rgbButtons));
        return g_ControllerData;
    }
}

u16 Controller::GetInput(void)
{
    u8 keyboardState[256];
    u16 buttons;

    buttons = 0;

    if (g_Supervisor.keyboard == NULL)
    {
        GetKeyboardState(keyboardState);

        buttons |= KEYBOARD_KEY_PRESSED(TH_BUTTON_UP, VK_UP);
        buttons |= KEYBOARD_KEY_PRESSED(TH_BUTTON_DOWN, VK_DOWN);
        buttons |= KEYBOARD_KEY_PRESSED(TH_BUTTON_LEFT, VK_LEFT);
        buttons |= KEYBOARD_KEY_PRESSED(TH_BUTTON_RIGHT, VK_RIGHT);
        buttons |= KEYBOARD_KEY_PRESSED(TH_BUTTON_UP, 'W');
        buttons |= KEYBOARD_KEY_PRESSED(TH_BUTTON_DOWN, 'S');
        buttons |= KEYBOARD_KEY_PRESSED(TH_BUTTON_LEFT, 'A');
        buttons |= KEYBOARD_KEY_PRESSED(TH_BUTTON_RIGHT, 'D');
        buttons |= KEYBOARD_KEY_PRESSED(TH_BUTTON_UP, VK_NUMPAD8);
        buttons |= KEYBOARD_KEY_PRESSED(TH_BUTTON_DOWN, VK_NUMPAD2);
        buttons |= KEYBOARD_KEY_PRESSED(TH_BUTTON_LEFT, VK_NUMPAD4);
        buttons |= KEYBOARD_KEY_PRESSED(TH_BUTTON_RIGHT, VK_NUMPAD6);
        buttons |= KEYBOARD_KEY_PRESSED(TH_BUTTON_UP_LEFT, VK_NUMPAD7);
        buttons |= KEYBOARD_KEY_PRESSED(TH_BUTTON_UP_RIGHT, VK_NUMPAD9);
        buttons |= KEYBOARD_KEY_PRESSED(TH_BUTTON_DOWN_LEFT, VK_NUMPAD1);
        buttons |= KEYBOARD_KEY_PRESSED(TH_BUTTON_DOWN_RIGHT, VK_NUMPAD3);
        buttons |= KEYBOARD_KEY_PRESSED(TH_BUTTON_HOME, VK_HOME);
        buttons |= KEYBOARD_KEY_PRESSED(TH_BUTTON_HOME, 'R');
        buttons |= KEYBOARD_KEY_PRESSED(TH_BUTTON_SHOOT, 'Z');
        buttons |= KEYBOARD_KEY_PRESSED(TH_BUTTON_BOMB, 'X');
        buttons |= KEYBOARD_KEY_PRESSED(TH_BUTTON_FOCUS, VK_SHIFT);
        buttons |= KEYBOARD_KEY_PRESSED(TH_BUTTON_MENU, VK_ESCAPE);
        buttons |= KEYBOARD_KEY_PRESSED(TH_BUTTON_SKIP, VK_CONTROL);
        buttons |= KEYBOARD_KEY_PRESSED(TH_BUTTON_Q, 'Q');
        buttons |= KEYBOARD_KEY_PRESSED(TH_BUTTON_S, 'S');
        if (!(GetKeyState(VK_MENU) & 0x8000))
        {
            buttons |= KEYBOARD_KEY_PRESSED(TH_BUTTON_ENTER, VK_RETURN);
        }
    }
    else
    {
        memset(keyboardState, 0, sizeof(keyboardState));
        HRESULT res = g_Supervisor.keyboard->GetDeviceState(sizeof(keyboardState), keyboardState);

        buttons = 0;

        if (FAILED(res))
        {
            memset(keyboardState, 0, sizeof(keyboardState));
            if (res == DIERR_INPUTLOST || res == DIERR_NOTACQUIRED)
                g_Supervisor.keyboard->Acquire();
        }

        buttons |= KEYBOARD_KEY_PRESSED(TH_BUTTON_UP_LEFT, DIK_NUMPAD7);
        buttons |= KEYBOARD_KEY_PRESSED(TH_BUTTON_UP_RIGHT, DIK_NUMPAD9);
        buttons |= KEYBOARD_KEY_PRESSED(TH_BUTTON_DOWN_LEFT, DIK_NUMPAD1);
        buttons |= KEYBOARD_KEY_PRESSED(TH_BUTTON_DOWN_RIGHT, DIK_NUMPAD3);
        buttons |= KEYBOARD_KEY_PRESSED(TH_BUTTON_HOME, DIK_HOME);
        buttons |= KEYBOARD_KEY_PRESSED(TH_BUTTON_HOME, DIK_R);

        if (g_KeyConfig.up || g_KeyConfig.down || g_KeyConfig.left || g_KeyConfig.right || g_KeyConfig.shoot ||
            g_KeyConfig.bomb || g_KeyConfig.focus || g_KeyConfig.menu || g_KeyConfig.skip)
        {
            if (g_KeyConfig.shoot)
                buttons |= KEYBOARD_KEY_PRESSED(TH_BUTTON_SHOOT, g_KeyConfig.shoot);
            if (g_KeyConfig.bomb)
                buttons |= KEYBOARD_KEY_PRESSED(TH_BUTTON_BOMB, g_KeyConfig.bomb);
            if (g_KeyConfig.focus)
                buttons |= KEYBOARD_KEY_PRESSED(TH_BUTTON_FOCUS, g_KeyConfig.focus);
            if (g_KeyConfig.menu)
                buttons |= KEYBOARD_KEY_PRESSED(TH_BUTTON_MENU, g_KeyConfig.menu);
            if (g_KeyConfig.up)
                buttons |= KEYBOARD_KEY_PRESSED(TH_BUTTON_UP, g_KeyConfig.up);
            if (g_KeyConfig.down)
                buttons |= KEYBOARD_KEY_PRESSED(TH_BUTTON_DOWN, g_KeyConfig.down);
            if (g_KeyConfig.left)
                buttons |= KEYBOARD_KEY_PRESSED(TH_BUTTON_LEFT, g_KeyConfig.left);
            if (g_KeyConfig.right)
                buttons |= KEYBOARD_KEY_PRESSED(TH_BUTTON_RIGHT, g_KeyConfig.right);
            if (g_KeyConfig.skip)
                buttons |= KEYBOARD_KEY_PRESSED(TH_BUTTON_SKIP, g_KeyConfig.skip);
        }
        else
        {

            buttons |= KEYBOARD_KEY_PRESSED(TH_BUTTON_UP, DIK_UP);
            buttons |= KEYBOARD_KEY_PRESSED(TH_BUTTON_DOWN, DIK_DOWN);
            buttons |= KEYBOARD_KEY_PRESSED(TH_BUTTON_LEFT, DIK_LEFT);
            buttons |= KEYBOARD_KEY_PRESSED(TH_BUTTON_RIGHT, DIK_RIGHT);
            buttons |= KEYBOARD_KEY_PRESSED(TH_BUTTON_UP, DIK_W);
            buttons |= KEYBOARD_KEY_PRESSED(TH_BUTTON_DOWN, DIK_S);
            buttons |= KEYBOARD_KEY_PRESSED(TH_BUTTON_LEFT, DIK_A);
            buttons |= KEYBOARD_KEY_PRESSED(TH_BUTTON_RIGHT, DIK_D);
            buttons |= KEYBOARD_KEY_PRESSED(TH_BUTTON_UP, DIK_NUMPAD8);
            buttons |= KEYBOARD_KEY_PRESSED(TH_BUTTON_DOWN, DIK_NUMPAD2);
            buttons |= KEYBOARD_KEY_PRESSED(TH_BUTTON_LEFT, DIK_NUMPAD4);
            buttons |= KEYBOARD_KEY_PRESSED(TH_BUTTON_RIGHT, DIK_NUMPAD6);
            buttons |= KEYBOARD_KEY_PRESSED(TH_BUTTON_SHOOT, DIK_Z);
            buttons |= KEYBOARD_KEY_PRESSED(TH_BUTTON_BOMB, DIK_X);
            buttons |= KEYBOARD_KEY_PRESSED(TH_BUTTON_FOCUS, DIK_LSHIFT);
            buttons |= KEYBOARD_KEY_PRESSED(TH_BUTTON_MENU, DIK_ESCAPE);
            buttons |= KEYBOARD_KEY_PRESSED(TH_BUTTON_SKIP, DIK_LCONTROL);
        }
        buttons |= KEYBOARD_KEY_PRESSED(TH_BUTTON_Q, DIK_Q);
        buttons |= KEYBOARD_KEY_PRESSED(TH_BUTTON_S, DIK_S);
        if (!(keyboardState[DIK_LALT] & 0x80) && !(keyboardState[DIK_RALT] & 0x80))
        {
            buttons |= KEYBOARD_KEY_PRESSED(TH_BUTTON_ENTER, DIK_RETURN);
        }
    }

    return Controller::GetControllerInput(buttons);
}

void Controller::ResetKeyboard(void)
{
    u8 key_states[256];

    GetKeyboardState(key_states);
    for (i32 idx = 0; idx < 256; idx++)
    {
        *(key_states + idx) &= 0x7f;
    }
    SetKeyboardState(key_states);
}
}; // namespace th06
