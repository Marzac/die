/**
    DIE ENGINE
    Depth Integration Engine / A modern ray-caster
    (c) Fred's Lab 2024-2026
    Frédéric Meslin / info@fredslab.net
    SPDX-License-Identifier: MIT
    If used commercially, contributions, donations are highly appreciated.

    Game controllers (xinput / evdev)
*/

#include "gamepad.h"

#include <algorithm>

Gamepad gamepads[GAMEPAD_DEVICES_MAX] = {Gamepad(0), Gamepad(1), Gamepad(2), Gamepad(3),};

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

#ifdef _WIN32

#include <windows.h>
#include <xinput.h>

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

#elif defined(__unix__) || defined(__unix)

#include <stdio.h>
#include <string.h>
#include <errno.h>

#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/input.h>

/*****************************************************************************/
namespace {
    constexpr int padsMax = 4;
    constexpr int devicesMax = 32;
    constexpr int axesMax = 4;
    constexpr int rescanPeriod = 300;    // in frames, when a pad is missing

    constexpr uint16_t axisCodes[axesMax] = {ABS_X, ABS_Y, ABS_RX, ABS_RY};
    constexpr uint16_t buttonCodes[] = {
        BTN_SOUTH, BTN_EAST, BTN_WEST, BTN_NORTH,
        BTN_START, BTN_SELECT, BTN_THUMBL, BTN_THUMBR,
        BTN_TL, BTN_TR
    };

    struct PadDevice {
        int fd = -1;
        int node = -1;
        int effect = -1;
        bool rumble = false;
        int32_t axesMin[axesMax] = {};
        float axesScale[axesMax] = {};
        int16_t axes[axesMax] = {};
        uint32_t buttons = 0;
    };

    PadDevice pads[padsMax];
    int rescanCountdown = 0;

/*****************************************************************************/
    bool testBit(int bit, const uint8_t * bits)
    {
        return (bits[bit >> 3] >> (bit & 7)) & 1;
    }

    bool isGamepad(int fd)
    {
        uint8_t keys[KEY_MAX / 8 + 1] = {};
        if (ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(keys)), keys) < 0) return false;
        return testBit(BTN_GAMEPAD, keys);
    }

    bool hasRumble(int fd)
    {
        uint8_t effects[FF_MAX / 8 + 1] = {};
        if (ioctl(fd, EVIOCGBIT(EV_FF, sizeof(effects)), effects) < 0) return false;
        return testBit(FF_RUMBLE, effects);
    }

    void initEffects(int fd)
    {
    // Set the maximum effect gain
        input_event event = {};
        event.type = EV_FF;
        event.code = FF_GAIN;
        event.value = 0xFFFF;
        write(fd, &event, sizeof(event));
    }

/*****************************************************************************/
    uint32_t mapButton(uint16_t code)
    {
        switch (code) {
        case BTN_SOUTH: return GAMEPAD_BUTTON_A;
        case BTN_EAST: return GAMEPAD_BUTTON_B;
        case BTN_WEST: return GAMEPAD_BUTTON_X;
        case BTN_NORTH: return GAMEPAD_BUTTON_Y;
        case BTN_START: return GAMEPAD_BUTTON_START;
        case BTN_SELECT: return GAMEPAD_BUTTON_BACK;
        case BTN_THUMBL: return GAMEPAD_BUTTON_LEFT_THUMB;
        case BTN_THUMBR: return GAMEPAD_BUTTON_RIGHT_THUMB;
        case BTN_TL: return GAMEPAD_BUTTON_LEFT_SHOULDER;
        case BTN_TR: return GAMEPAD_BUTTON_RIGHT_SHOULDER;
        default: return 0;
        }
    }

    void mapHat(uint32_t & buttons, int32_t value, uint32_t neg, uint32_t pos)
    {
        buttons &= ~(neg | pos);
        if (value < 0) buttons |= neg;
        if (value > 0) buttons |= pos;
    }

    int16_t rescaleAxis(const PadDevice & pad, int axis, int32_t value)
    {
        float v = (value - pad.axesMin[axis]) * pad.axesScale[axis] - 32768.0f;
        return (int16_t) std::clamp(v, -32768.0f, 32767.0f);
    }

/*****************************************************************************/
    void initState(PadDevice & pad, int fd)
    {
    // Read the axis ranges and positions
        for (int a = 0; a < axesMax; a++) {
            pad.axesMin[a] = -32768;
            pad.axesScale[a] = 1.0f;
            pad.axes[a] = 0;
            input_absinfo info = {};
            if (ioctl(fd, EVIOCGABS(axisCodes[a]), &info) < 0) continue;
            if (info.maximum <= info.minimum) continue;
            pad.axesMin[a] = info.minimum;
            pad.axesScale[a] = 65535.0f / (info.maximum - info.minimum);
            pad.axes[a] = rescaleAxis(pad, a, info.value);
        }

    // Read the button states
        pad.buttons = 0;
        uint8_t keys[KEY_MAX / 8 + 1] = {};
        if (ioctl(fd, EVIOCGKEY(sizeof(keys)), keys) >= 0)
            for (uint16_t code : buttonCodes)
                if (testBit(code, keys)) pad.buttons |= mapButton(code);

        input_absinfo hat = {};
        if (ioctl(fd, EVIOCGABS(ABS_HAT0X), &hat) >= 0)
            mapHat(pad.buttons, hat.value, GAMEPAD_BUTTON_DPAD_LEFT, GAMEPAD_BUTTON_DPAD_RIGHT);
        if (ioctl(fd, EVIOCGABS(ABS_HAT0Y), &hat) >= 0)
            mapHat(pad.buttons, hat.value, GAMEPAD_BUTTON_DPAD_UP, GAMEPAD_BUTTON_DPAD_DOWN);
    }

    void scanDevices()
    {
        if (rescanCountdown-- > 0) return;
        rescanCountdown = rescanPeriod;

        for (int node = 0; node < devicesMax; node++) {
        // Look for a free pad slot
            int slot = -1;
            for (int p = 0; p < padsMax; p++)
                if (pads[p].fd < 0) {slot = p; break;}
            if (slot < 0) return;

        // Skip the devices already in use
            bool used = false;
            for (int p = 0; p < padsMax; p++)
                if (pads[p].node == node) used = true;
            if (used) continue;

        // Open the event device (read-only when rumble is not permitted)
            char path[64] = "";
            snprintf(path, sizeof(path), "/dev/input/event%i", node);
            int fd = open(path, O_RDWR | O_NONBLOCK);
            bool writable = fd >= 0;
            if (fd < 0) fd = open(path, O_RDONLY | O_NONBLOCK);
            if (fd < 0) continue;
            if (!isGamepad(fd)) {close(fd); continue;}

            PadDevice & pad = pads[slot];
            pad.rumble = writable && hasRumble(fd);
            if (pad.rumble) initEffects(fd);
            initState(pad, fd);
            pad.fd = fd;
            pad.node = node;
        }
    }

    void closeDevice(int slot)
    {
        close(pads[slot].fd);
        pads[slot] = PadDevice();
    }
}

/*****************************************************************************/
void Gamepad::update()
{
    if (id < 0 || id >= padsMax) return;
    PadDevice & pad = pads[id];

// Look for new controllers
    if (pad.fd < 0) scanDevices();

// Load the state of a freshly connected controller
    if (pad.fd >= 0 && !detected) {
        buttons = pad.buttons;
        leftX = filterAxis(pad.axes[0]);
        leftY = -filterAxis(pad.axes[1]);
        rightX = filterAxis(pad.axes[2]);
        rightY = -filterAxis(pad.axes[3]);
    }
    detected = pad.fd >= 0;
    uint32_t lastButtons = buttons;

// Drain the pending events
    if (detected) while (true) {
        input_event event;
        ssize_t bytes = read(pad.fd, &event, sizeof(event));
        if (bytes != (ssize_t) sizeof(event)) {
            if (bytes < 0 && errno == EAGAIN) break;
        // The controller was unplugged
            closeDevice(id);
            detected = false;
            leftX = leftY = 0.0f;
            rightX = rightY = 0.0f;
            buttons = 0;
            break;
        }

        if (event.type == EV_KEY) {
            uint32_t b = mapButton(event.code);
            if (event.value) buttons |= b;
            else buttons &= ~b;
        }else if (event.type == EV_ABS) {
            switch (event.code) {
            case ABS_X: leftX = filterAxis(rescaleAxis(pad, 0, event.value)); break;
            case ABS_Y: leftY = -filterAxis(rescaleAxis(pad, 1, event.value)); break;
            case ABS_RX: rightX = filterAxis(rescaleAxis(pad, 2, event.value)); break;
            case ABS_RY: rightY = -filterAxis(rescaleAxis(pad, 3, event.value)); break;
            case ABS_HAT0X:
                mapHat(buttons, event.value, GAMEPAD_BUTTON_DPAD_LEFT, GAMEPAD_BUTTON_DPAD_RIGHT);
                break;
            case ABS_HAT0Y:
                mapHat(buttons, event.value, GAMEPAD_BUTTON_DPAD_UP, GAMEPAD_BUTTON_DPAD_DOWN);
                break;
            default: break;
            }
        }
    }

    toggled = lastButtons ^ buttons;
    pressed = toggled & buttons;
    released = toggled & ~buttons;
}

void Gamepad::feedback(float left, float right)
{
    if (id < 0 || id >= padsMax) return;
    PadDevice & pad = pads[id];
    if (pad.fd < 0 || !pad.rumble) return;

// Register or update the effect
    ff_effect effect = {};
    effect.type = FF_RUMBLE;
    effect.id = pad.effect;
    effect.replay.length = 0x7FFF;
    effect.u.rumble.strong_magnitude = (uint16_t) (std::clamp(left, 0.0f, 1.0f) * 65535.0f);
    effect.u.rumble.weak_magnitude = (uint16_t) (std::clamp(right, 0.0f, 1.0f) * 65535.0f);
    if (ioctl(pad.fd, EVIOCSFF, &effect) < 0) return;
    pad.effect = effect.id;

// Start or stop the playback
    input_event play = {};
    play.type = EV_FF;
    play.code = (uint16_t) effect.id;
    play.value = left > 0.0f || right > 0.0f;
    write(pad.fd, &play, sizeof(play));
}

#endif // _WIN32
