/**
    DIE ENGINE
    Depth Integration Engine / A modern ray-caster
    (c) Fred's Lab 2024-2026
    Frédéric Meslin / info@fredslab.net
    SPDX-License-Identifier: MIT
    If used commercially, contributions, donations are highly appreciated.

    environment properties
*/

#include "env.h"
#include "renderer.h"

#include <QVector3D>

#include <math.h>

/*****************************************************************************/
Env::Env() :
    sun{}, fog{}
{
    init();
}

/*****************************************************************************/
void Env::init()
{
    sun.hour = 9.0f;
    sun.angle = 1.0f;
    sun.ambientColor = 0xBBBBBB;
    sun.ambientStrength = 1.0f;
    sun.rayColor = 0xFFFFDD;
    sun.rayStrength = 1.0f;

    fog.distanceNear = 8.0f;
    fog.distanceFar = 320.0f;
    fog.color = 0x000000;
    fog.flags = FOG_FLAG_ENABLE;
}

void Env::terminate()
{

}

/*****************************************************************************/
void Env::pass()
{
    constexpr float pi = 3.1415926f;
    float ha = pi * sun.hour / 12.0f;
    float da = pi * sun.angle / 12.0f;

    renderer.sunRayDirection = -QVector3D(sinf(ha) * cosf(da), -cosf(ha) * cosf(da), sinf(da));
    renderer.sunRayStrength = sun.rayStrength;
    renderer.sunAmbient = sun.ambientColor;
    renderer.sunAmbientStrength = sun.ambientStrength;

    renderer.fogDistanceNear = fog.distanceNear;
    renderer.fogDistanceFar = fog.distanceFar;
    renderer.fogFarColor = fog.color;
    renderer.fogFlags = fog.flags;
}
