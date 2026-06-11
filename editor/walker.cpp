/**
    WALLER
    Map editor for the DIE engine
    (c) Fred's Lab 2024-2026
    Frédéric Meslin / info@fredslab.net
    SPDX-License-Identifier: MIT
    If used commercially, contributions, donations are highly appreciated.

    first-person walker
*/

#include "walker.h"

#include "renderer.h"
#include "audio.h"
#include "editor.h"
#include "gamepad.h"

#include <math.h>

Walker walker;

/*****************************************************************************/
Walker::Walker() :
    pos(), speed(), accel(),
    pan(0.0f), tilt(0.0f),
    pitchTargetPad(0.0f), pitchTargetMouse(0.0f),
    jumpCount(0), onFloor(false), isFlying(true)
{
    init();
}

/*****************************************************************************/
void Walker::init()
{
    pos = QVector3D(0.0f, 12.0f, 0.0f);
    speed = QVector3D();
    accel = QVector3D();

    pan = 0.0f;
    tilt = 0.0f;
    pitchTargetPad = 0.0f;
    pitchTargetMouse = 0.0f;

    jumpCount = 0;
    onFloor = false;
    isFlying = true;
}

void Walker::terminate()
{

}

/*****************************************************************************/
void Walker::update()
{
    gamepad1.update();
    updateMove();

    if (audio.audioListener) {
        audio.audioListener->setPosition(pos);
        audio.audioListener->setRotation(QQuaternion::fromAxisAndAngle(QVector3D(0.0f, 1.0f, 0.0f), pan));
    }

    editor.viewPoint.pos = pos;
    editor.viewPoint.pan = pan * D2R;
    editor.viewPoint.tilt = tilt * D2R;
}

/*****************************************************************************/
void Walker::updateMove()
{
    pan += speedPan * gamepad1.leftX * R2D;
    pan = fmodf(pan, 360.0f);

    pitchTargetPad = gamepad1.rightY * cameraPitch;
    float pitchTarget = pitchTargetPad + pitchTargetMouse;
    tilt = pitchTarget - (pitchTarget - tilt) * 0.8f;

    QVector3D runVector(cosf(pan * D2R), 0.0f, sinf(pan * D2R));
    float thrustRun = speedRun * gamepad1.leftY;
    accel = runVector * thrustRun;

    QVector3D strafeVector(-runVector.z(), 0.0f, runVector.x());
    float thrustStrafe = speedStrafe * gamepad1.rightX;
    accel += strafeVector * thrustStrafe;

    if ((fabsf(thrustRun) > 0.1f) || (fabsf(thrustStrafe) > 0.1f)) {
        speed = accel;
    }

    updateGravity();
    updateCollisions();

    pos += speed;
    speed *= 0.60f;
}

/*****************************************************************************/
void Walker::updateGravity()
{
/*
    Player & player = players[playerID];
    if (onFloor) {
        if (gamepad1.buttons & GAMEPAD_BUTTON_A) {
            jumpCount = 5;
            onFloor = false;
        }
    }else speed -= QVector3D(0.0f, 0.5f, 0.0f);

    if (jumpCount) {
        speed.setY(1.25f);
        jumpCount--;
    }

    if (pos.y() <= 0.0f) {
        pos.setY(0.0f);
        speed.setY(0.0f);
        onFloor = true;
    }
*/
}

void Walker::updateCollisions()
{
    if (!editor.collisions) return;

    QVector2D res = QVector2D(pos.x(), pos.z());

    int passes = 10;
    while (passes--) {
        bool resolved = true;

        for (int i = 0; i < renderer.wallsCount; i++) {
            auto & w = renderer.walls[i];

            if (w.flags & WALL_FLAG_INVISIBLE) continue;
            float wBot = renderer.nodes[w.nodeID1].pos.y();
            if (pos.y() + radius < wBot) continue;
            float wTop = wBot + w.height;
            if (pos.y() - radius > wTop) continue;

            QVector2D result;
            if (!intersect(res, radius, w, result)) continue;
            res += result;
            resolved = false;
        }

        if (resolved) break;
    }

    pos.setX(res.x());
    pos.setZ(res.y());
}

/*****************************************************************************/
bool Walker::intersect(const QVector2D & pos, float radius, const Renderer::Wall & w, QVector2D & result)
{
    auto & n1 = renderer.nodes[w.nodeID1].pos;
    auto & n2 = renderer.nodes[w.nodeID2].pos;

    QVector2D p1(n1.x(), n1.z());
    QVector2D p2(n2.x(), n2.z());
    QVector2D vec = p1 - pos;
    QVector2D dir = (p2 - p1).normalized();

    float cross = dir.x() * vec.y() - dir.y() * vec.x();
    if (w.flags & WALL_FLAG_BACKCULLED && cross <= 0.0f) return false;
    float dist = fabsf(cross);
    if (dist >= radius) return false;

    float b = 2.0f * QVector2D::dotProduct(vec, dir);
    float c = QVector2D::dotProduct(vec, vec) - radius * radius;
    float delta = sqrtf(b * b - 4.0f * c);

    float len = (p2 - p1).length();
    float t1 = (-b - delta) / 2.0f;
    float t2 = (-b + delta) / 2.0f;
    bool k1In = t1 >= 0.0f && t1 <= len;
    bool k2In = t2 >= 0.0f && t2 <= len;

    if (!k1In && !k2In) return false;
    QVector2D ret = QVector2D(dir.y(), -dir.x());
    result = ret * std::copysignf(radius - dist, cross);
    return true;
}

/*
}else if (k1In) {
    QVector2D k1 = p1 + dir * t1;
    QVector2D ret = (k1 - pos).normalized();
    result = ret * (cross > 0.0f ? (radius - dist) : (dist - radius));
    return true;

}else if (k2In) {
    QVector2D k2 = p1 + dir * t2;
    QVector2D ret = (k2 - pos).normalized();
    result = ret * (cross > 0.0f ? (radius - dist) : (dist - radius));
    return true;
}
*/
