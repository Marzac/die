/**
    DIE ENGINE
    Depth Integration Engine / A modern ray-caster
    (c) Fred's Lab 2024-2026
    Frédéric Meslin / info@fredslab.net
    SPDX-License-Identifier: MIT
    If used commercially, contributions, donations are highly appreciated.

    easing functions
    Based on: https://github.com/nicolausYes/easing-functions
*/

#ifndef EASINGS_H
#define EASINGS_H

#include <math.h>
#include <stdint.h>

typedef enum : uint16_t {
    EASING_LINEAR = 0,
    EASING_IN_SINE,     EASING_OUT_SINE,     EASING_IN_OUT_SINE,
    EASING_IN_QUAD,     EASING_OUT_QUAD,     EASING_IN_OUT_QUAD,
    EASING_IN_CUBIC,    EASING_OUT_CUBIC,    EASING_IN_OUT_CUBIC,
    EASING_IN_QUART,    EASING_OUT_QUART,    EASING_IN_OUT_QUART,
    EASING_IN_QUINT,    EASING_OUT_QUINT,    EASING_IN_OUT_QUINT,
    EASING_IN_EXPO,     EASING_OUT_EXPO,     EASING_IN_OUT_EXPO,
    EASING_IN_CIRC,     EASING_OUT_CIRC,     EASING_IN_OUT_CIRC,
    EASING_IN_BACK,     EASING_OUT_BACK,     EASING_IN_OUT_BACK,
    EASING_IN_ELASTIC,  EASING_OUT_ELASTIC,  EASING_IN_OUT_ELASTIC,
    EASING_IN_BOUNCE,   EASING_OUT_BOUNCE,   EASING_IN_OUT_BOUNCE,
    EASING_COUNT,
} EASING_TYPES;

/*****************************************************************************/
static inline float easeInSine(float t)
{
    return sinf(1.5707963f * t);
}

static inline float easeOutSine(float t)
{
    return 1.0f + sinf(1.5707963f * (t - 1.0f));
}

static inline float easeInOutSine(float t)
{
    return 0.5f * (1.0f + sinf(3.1415926f * (t - 0.5f)));
}

static inline float easeInQuad(float t)
{
    return t * t;
}

static inline float easeOutQuad(float t)
{
    return t * (2.0f - t);
}

static inline float easeInOutQuad(float t)
{
    return (t < 0.5f) ?
        2.0f * t * t :
        t * (4.0f - 2.0f * t) - 1.0f;
}

static inline float easeInCubic(float t)
{
    return t * t * t;
}

static inline float easeOutCubic(float t)
{
    t -= 1.0f;
    return 1.0f + t * t * t;
}

static inline float easeInOutCubic(float t)
{
    if (t < 0.5f) {
        return 4.0f * t * t * t;
    } else {
        t -= 1.0f;
        return 1.0f + 4.0f * t * t * t;
    }
}

static inline float easeInQuart(float t)
{
    t *= t;
    return t * t;
}

static inline float easeOutQuart(float t)
{
    t = (t - 1.0f) * (t - 1.0f);
    return 1.0f - t * t;
}

static inline float easeInOutQuart(float t)
{
    if (t < 0.5f) {
        t *= t;
        return 8.0f * t * t;
    } else {
        t = (t - 1.0f) * (t - 1.0f);
        return 1.0f - 8.0f * t * t;
    }
}

static inline float easeInQuint(float t)
{
    float t2 = t * t;
    return t * t2 * t2;
}

static inline float easeOutQuint(float t)
{
    t -= 1.0f;
    float t2 = t * t;
    return 1.0f + t * t2 * t2;
}

static inline float easeInOutQuint(float t)
{
    float t2;
    if (t < 0.5f) {
        t2 = t * t;
        return 16.0f * t * t2 * t2;
    } else {
        t -= 1.0f;
        t2 = t * t;
        return 1.0f + 16.0f * t * t2 * t2;
    }
}

static inline float easeInExpo(float t)
{
    return (powf(2.0f, 8.0f * t) - 1.0f) / 256.0f;
}

static inline float easeOutExpo(float t)
{
    return 1.0f - powf(2.0f, -8.0f * t);
}

static inline float easeInOutExpo(float t)
{
    if (t < 0.5f) {
        return (powf(2.0f, 16.0f * t) - 1.0f) / 512.0f;
    } else {
        return 1.0f - 0.5f * powf(2.0f, -16.0f * (t - 0.5f));
    }
}

static inline float easeInCirc(float t)
{
    return 1.0f - sqrtf(1.0f - t);
}

static inline float easeOutCirc(float t)
{
    return sqrtf(t);
}

static inline float easeInOutCirc(float t)
{
    if (t < 0.5f) {
        return (1.0f - sqrtf(1.0f - 2.0f * t)) * 0.5f;
    } else {
        return (1.0f + sqrtf(2.0f * t - 1.0f)) * 0.5f;
    }
}

static inline float easeInBack(float t)
{
    return t * t * (2.70158f * t - 1.70158f);
}

static inline float easeOutBack(float t)
{
    t -= 1.0f;
    return 1.0f + t * t * (2.70158f * t + 1.70158f);
}

static inline float easeInOutBack(float t)
{
    if (t < 0.5f) {
        return t * t * (7.0f * t - 2.5f) * 2.0f;
    } else {
        t -= 1.0f;
        return 1.0f + t * t * 2.0f * (7.0f * t + 2.5f);
    }
}

static inline float easeInElastic(float t)
{
    float t2 = t * t;
    return t2 * t2 * sinf(t * 3.1415926f * 4.5f);
}

static inline float easeOutElastic(float t)
{
    float t2 = (t - 1.0f) * (t - 1.0f);
    return 1.0f - t2 * t2 * cosf(t * 3.1415926f * 4.5f);
}

static inline float easeInOutElastic(float t)
{
    float t2;
    if (t < 0.45f) {
        t2 = t * t;
        return 8.0f * t2 * t2 * sinf(t * 3.1415926f * 9.0f);
    } else if (t < 0.55f) {
        return 0.5f + 0.75f * sinf(t * 3.1415926f * 4.0f);
    } else {
        t2 = (t - 1.0f) * (t - 1.0f);
        return 1.0f - 8.0f * t2 * t2 * sinf(t * 3.1415926f * 9.0f);
    }
}

static inline float easeInBounce(float t)
{
    return powf(2.0f, 6.0f * (t - 1.0f)) * fabsf(sinf(t * 3.1415926f * 3.5f));
}

static inline float easeOutBounce(float t)
{
    return 1.0f - powf(2.0f, -6.0f * t) * fabsf(cosf(t * 3.1415926f * 3.5f));
}

static inline float easeInOutBounce(float t)
{
    if (t < 0.5f) {
        return 8.0f * powf(2.0f, 8.0f * (t - 1.0f)) * fabsf(sinf(t * 3.1415926f * 7.0f));
    } else {
        return 1.0f - 8.0f * powf(2.0f, -8.0f * t) * fabsf(sinf(t * 3.1415926f * 7.0f));
    }
}

/*****************************************************************************/
/**
    \brief Apply an easing curve to a normalised time value
    \param type easing curve (one of EASING_TYPES)
    \param t normalised time (0.0 .. 1.0)
    \return eased value (0.0 .. 1.0, back / elastic curves may overshoot)
*/
static inline float applyEasing(uint16_t type, float t)
{
    switch (type) {
        default:
        case EASING_LINEAR:          return t;
        case EASING_IN_SINE:         return easeInSine(t);
        case EASING_OUT_SINE:        return easeOutSine(t);
        case EASING_IN_OUT_SINE:     return easeInOutSine(t);
        case EASING_IN_QUAD:         return easeInQuad(t);
        case EASING_OUT_QUAD:        return easeOutQuad(t);
        case EASING_IN_OUT_QUAD:     return easeInOutQuad(t);
        case EASING_IN_CUBIC:        return easeInCubic(t);
        case EASING_OUT_CUBIC:       return easeOutCubic(t);
        case EASING_IN_OUT_CUBIC:    return easeInOutCubic(t);
        case EASING_IN_QUART:        return easeInQuart(t);
        case EASING_OUT_QUART:       return easeOutQuart(t);
        case EASING_IN_OUT_QUART:    return easeInOutQuart(t);
        case EASING_IN_QUINT:        return easeInQuint(t);
        case EASING_OUT_QUINT:       return easeOutQuint(t);
        case EASING_IN_OUT_QUINT:    return easeInOutQuint(t);
        case EASING_IN_EXPO:         return easeInExpo(t);
        case EASING_OUT_EXPO:        return easeOutExpo(t);
        case EASING_IN_OUT_EXPO:     return easeInOutExpo(t);
        case EASING_IN_CIRC:         return easeInCirc(t);
        case EASING_OUT_CIRC:        return easeOutCirc(t);
        case EASING_IN_OUT_CIRC:     return easeInOutCirc(t);
        case EASING_IN_BACK:         return easeInBack(t);
        case EASING_OUT_BACK:        return easeOutBack(t);
        case EASING_IN_OUT_BACK:     return easeInOutBack(t);
        case EASING_IN_ELASTIC:      return easeInElastic(t);
        case EASING_OUT_ELASTIC:     return easeOutElastic(t);
        case EASING_IN_OUT_ELASTIC:  return easeInOutElastic(t);
        case EASING_IN_BOUNCE:       return easeInBounce(t);
        case EASING_OUT_BOUNCE:      return easeOutBounce(t);
        case EASING_IN_OUT_BOUNCE:   return easeInOutBounce(t);
    }
}

#endif // EASINGS_H
