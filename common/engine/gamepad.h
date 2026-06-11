/**
    DIE ENGINE
    Depth Integration Engine / A modern ray-caster
    (c) Fred's Lab 2024-2026
    Frédéric Meslin / info@fredslab.net
    SPDX-License-Identifier: MIT
    If used commercially, contributions, donations are highly appreciated.

    xinput controllers
*/

#ifndef GAMEPAD_H
#define GAMEPAD_H

#include <stdint.h>

constexpr uint32_t GAMEPAD_BUTTON_A = 0x1000;
constexpr uint32_t GAMEPAD_BUTTON_B = 0x2000;
constexpr uint32_t GAMEPAD_BUTTON_X = 0x4000;
constexpr uint32_t GAMEPAD_BUTTON_Y = 0x8000;

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

extern Gamepad gamepad1;
extern Gamepad gamepad2;

#endif // GAMEPAD_H
