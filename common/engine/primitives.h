/**
    RIGGER
    Animation editor for the DIE engine
    (c) Fred's Lab 2024-2026
    Frédéric Meslin / info@fredslab.net
    SPDX-License-Identifier: MIT
    If used commercially, contributions, donations are highly appreciated.

    graphic promitives
*/

#ifndef PRIMITIVES_H
#define PRIMITIVES_H

    #include <QVector2D>
    #include <QVector3D>
    #include <stdint.h>

    constexpr float D2R = 3.14159265f / 180.0f;
    constexpr float R2D = 180.0f / 3.14159265f;

    typedef struct {
        QVector3D pos;
        QVector3D offset;
        float pan;
        float tilt;
    } Viewpoint;

    typedef struct {
        float pan;          ///< rotation about the Y axis, in degrees
        float diameter;     ///< cylinder diameter / camera Z distance
        float y;            ///< vertical offset
    } ViewpointOrtho;

    typedef struct {
        uint32_t * pixels;
        uint16_t size;
        uint16_t count;
        uint16_t mask;
        uint32_t block;
    } Texture;

    typedef struct {
        float xs[4];
        float ys[4];
        float us[4];
        float vs[4];
        uint16_t surfaceId;
    }Quad;

    typedef struct {
        float xs[3];
        float ys[3];
        float us[3];
        float vs[3];
        uint16_t surfaceId;
    }Triangle;

#endif
