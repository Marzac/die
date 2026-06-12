/**
    DIE ENGINE
    Depth Integration Engine / A modern ray-caster
    (c) Fred's Lab 2024-2026
    Frédéric Meslin / info@fredslab.net
    SPDX-License-Identifier: MIT
    If used commercially, contributions, donations are highly appreciated.

    xinput controllers
*/

#include "gamepad.h"

#ifdef _WIN32

#include <windows.h>
#include <xinput.h>

#include <algorithm>

Gamepad gamepad1(0);
Gamepad gamepad2(1);

/*****************************************************************************/
Gamepad::Gamepad(int id) :
    id (id),
    leftX(0.0f), leftY(0.0f),
    rightX(0.0f), rightY(0.0f),
    buttons(0), toggled(0), pressed(0), released(0),
    detected(false)
{

}

/*****************************************************************************/
float Gamepad::filterAxis(int16_t value)
{
    constexpr int16_t threshold = 4096;
    constexpr float scale = 1.0f / (32768.0f - threshold);
    if (value > threshold) return (value - threshold) * scale;
    if (value < -threshold) return (value + threshold) * scale;
    return 0.0f;
}

/*****************************************************************************/
void Gamepad::update()
{
    XINPUT_STATE state = {};
    DWORD result = XInputGetState(id, &state);
    detected = result == ERROR_SUCCESS;

    leftX = filterAxis(state.Gamepad.sThumbLX);
    leftY = filterAxis(state.Gamepad.sThumbLY);
    rightX = filterAxis(state.Gamepad.sThumbRX);
    rightY = filterAxis(state.Gamepad.sThumbRY);

    toggled = buttons ^ state.Gamepad.wButtons;
    buttons = state.Gamepad.wButtons;
    pressed = toggled & buttons;
    released = toggled & ~buttons;
}

void Gamepad::feedback(float left, float right)
{
    XINPUT_VIBRATION force;
    force.wLeftMotorSpeed = (uint16_t) (std::clamp(left, 0.0f, 1.0f) * 65535.0f);
    force.wRightMotorSpeed = (uint16_t) (std::clamp(right, 0.0f, 1.0f) * 65535.0f);
    XInputSetState(id, &force);
}

#else // _WIN32
//#error "gamepad.cpp: only the Windows (XInput) implementation is available for now"

#include <algorithm>

Gamepad gamepad1(0);
Gamepad gamepad2(1);

/*****************************************************************************/
Gamepad::Gamepad(int id) :
    id (id),
    leftX(0.0f), leftY(0.0f),
    rightX(0.0f), rightY(0.0f),
    buttons(0), toggled(0), pressed(0), released(0),
    detected(false)
{

}

/*****************************************************************************/
float Gamepad::filterAxis(int16_t value)
{
}

/*****************************************************************************/
void Gamepad::update()
{
}

void Gamepad::feedback(float left, float right)
{
}

#endif // _WIN32
