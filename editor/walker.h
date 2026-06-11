/**
    WALLER
    Map editor for the DIE engine
    (c) Fred's Lab 2024-2026
    Frédéric Meslin / info@fredslab.net
    SPDX-License-Identifier: MIT
    If used commercially, contributions, donations are highly appreciated.

    first-person walker
*/

#ifndef WALKER_H
#define WALKER_H

#include "renderer.h"

#include <QVector2D>
#include <QVector3D>

class Walker
{
public:
    static constexpr float speedPan = 0.10f;
    static constexpr float speedRun = 0.45f;
    static constexpr float speedStrafe = 0.33f;
    static constexpr float radius = 2.0f;
    static constexpr float cameraPitch = 40.0f;    // max gamepad pitch, in degrees

    Walker();

    void init();
    void terminate();

    /// \brief Poll the gamepad, move the walker and update the editor viewpoint
    void update();

    QVector3D pos;
    QVector3D speed;
    QVector3D accel;
    float pan;
    float tilt;
    float pitchTargetPad;     ///< pitch from the gamepad
    float pitchTargetMouse;     ///< pitch from the mouse (set by the map view)

    int jumpCount;
    bool onFloor;
    bool isFlying;

private:
    void updateMove();
    void updateGravity();
    void updateCollisions();

    bool intersect(const QVector2D & pos, float radius, const Renderer::Wall & w, QVector2D & result);
};

extern Walker walker;

#endif // WALKER_H
