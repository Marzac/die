/**
    DIE ENGINE
    Depth Integration Engine / A modern ray-caster
    (c) Fred's Lab 2024-2026
    Frédéric Meslin / info@fredslab.net
    SPDX-License-Identifier: MIT
    If used commercially, contributions, donations are highly appreciated.

    environment properties
*/

#ifndef ENV_H
#define ENV_H

#include <stdint.h>

/*****************************************************************************/
/**
    \brief Sun lighting properties
    \note hour and angle are expressed in hours (12 = half a circle)
*/
typedef struct {
    float hour;             ///< sun azimuth, in hours (0 .. 24)
    float angle;            ///< sun tilt, in hours (0 .. 24)
    uint32_t ambientColor;  ///< packed RGB ambient color
    uint32_t rayColor;      ///< packed RGB direct ray color
    float ambientStrength;  ///< ambient intensity (0.0 .. 1.0)
    float rayStrength;      ///< ray intensity (0.0 .. 1.0)
} Sun;

typedef enum {
    FOG_FLAG_ENABLE = 0x0001,
} FOG_FLAGS;

/**
    \brief Distance fog properties
*/
typedef struct {
    float distanceNear;     ///< fog start distance, in world units
    float distanceFar;      ///< fog full-density distance, in world units
    uint32_t color;         ///< packed RGB fog color
    uint16_t flags;         ///< combination of FOG_FLAGS
} Fog;

/*****************************************************************************/
inline bool operator==(const Sun & a, const Sun & b)
{
    return a.hour == b.hour &&
           a.angle == b.angle &&
           a.ambientColor == b.ambientColor &&
           a.rayColor == b.rayColor &&
           a.ambientStrength == b.ambientStrength &&
           a.rayStrength == b.rayStrength;
}

inline bool operator==(const Fog & a, const Fog & b)
{
    return a.distanceNear == b.distanceNear &&
           a.distanceFar == b.distanceFar &&
           a.color == b.color &&
           a.flags == b.flags;
}

/*****************************************************************************/
class Env
{
public:
    Env();

    /// \brief Set the default sun & fog properties
    void init();

    /// \brief Release the environment resources
    void terminate();

    /// \brief Push the environment properties to the renderer
    void pass();

    Sun sun;
    Fog fog;
};

#endif // ENV_H
