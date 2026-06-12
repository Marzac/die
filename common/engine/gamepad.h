/**
    DIE ENGINE
    Depth Integration Engine / A modern ray-caster
    (c) Fred's Lab 2024-2026
    Frédéric Meslin / info@fredslab.net
    SPDX-License-Identifier: MIT
    If used commercially, contributions, donations are highly appreciated.

    Game controllers (xinput / evdev)
*/

#ifndef GAMEPAD_H
#define GAMEPAD_H

#include <stdint.h>

#define GAMEPAD_DEVICES_MAX 4

// Button masks, xinput bit layout
typedef enum : uint16_t {
    GAMEPAD_BUTTON_DPAD_UP = 0x0001,
    GAMEPAD_BUTTON_DPAD_DOWN = 0x0002,
    GAMEPAD_BUTTON_DPAD_LEFT = 0x0004,
    GAMEPAD_BUTTON_DPAD_RIGHT = 0x0008,
    GAMEPAD_BUTTON_START = 0x0010,
    GAMEPAD_BUTTON_BACK = 0x0020,
    GAMEPAD_BUTTON_LEFT_THUMB = 0x0040,
    GAMEPAD_BUTTON_RIGHT_THUMB = 0x0080,
    GAMEPAD_BUTTON_LEFT_SHOULDER = 0x0100,
    GAMEPAD_BUTTON_RIGHT_SHOULDER = 0x0200,
    GAMEPAD_BUTTON_A = 0x1000,
    GAMEPAD_BUTTON_B = 0x2000,
    GAMEPAD_BUTTON_X = 0x4000,
    GAMEPAD_BUTTON_Y = 0x8000,
} GAMEPAD_BUTTONS;

class Gamepad
{
public:
    Gamepad(int id);

    /// \brief Poll the controller state (axes, buttons, presence)
    void update();

    /**
        \brief Set the vibration motor speeds
        \param left low-frequency motor strength (0.0 .. 1.0)
        \param right high-frequency motor strength (0.0 .. 1.0)
    */
    void feedback(float left, float right);

    int id;
    float leftX;
    float leftY;
    float rightX;
    float rightY;

    uint32_t buttons;
    uint32_t toggled;
    uint32_t pressed;
    uint32_t released;

    bool detected;

private:
    /// \brief Apply the dead-zone and normalise a raw thumb-stick axis
    float filterAxis(int16_t value);
};

extern Gamepad gamepads[GAMEPAD_DEVICES_MAX];

#endif // GAMEPAD_H
