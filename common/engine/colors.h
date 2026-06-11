/**
    DIE ENGINE
    Depth Integration Engine / A modern ray-caster
    (c) Fred's Lab 2024-2026
    Frédéric Meslin / info@fredslab.net
    SPDX-License-Identifier: MIT
    If used commercially, contributions, donations are highly appreciated.

    color functions
*/

#ifndef COLORS_H
#define COLORS_H

#include <smmintrin.h>

#include <stdint.h>

/*****************************************************************************/
/**
    \brief Scale a packed ARGB color by a fixed-point factor
    \param color packed 8-bit ARGB color
    \param scale Q0.16 scale factor (0 .. 65535 = 0.0 .. ~1.0)
    \return scaled packed ARGB color
*/
inline uint32_t colorsScaleSSE4(uint32_t color, uint16_t scale)
{
// Load color and unpack bytes to words
    __m128i c = _mm_cvtsi32_si128((int) color);
    __m128i c_unpacked = _mm_cvtepu8_epi32(c);

// Multiply color by scale
    __m128i scale32 = _mm_set1_epi32(scale);
    __m128i result = _mm_mullo_epi32(c_unpacked, scale32);
    result = _mm_srli_epi32(result, 16);

// Pack back to bytes
    result = _mm_packus_epi32(result, result);
    result = _mm_packus_epi16(result, result);

    return _mm_cvtsi128_si32(result);
}

/**
    \brief Multiply two packed ARGB colors component-wise
    \param color1 packed 8-bit ARGB color
    \param color2 packed 8-bit ARGB color
    \return product packed ARGB color (components scaled by 1/256)
*/
inline uint32_t colorsMultiplySSE4(uint32_t color1, uint32_t color2)
{
// Load colors and unpack bytes to words
    __m128i c1 = _mm_cvtsi32_si128((int) color1);
    __m128i c2 = _mm_cvtsi32_si128((int) color2);
    __m128i c1_unpacked = _mm_cvtepu8_epi32(c1);
    __m128i c2_unpacked = _mm_cvtepu8_epi32(c2);

// Multiply the components
    __m128i result = _mm_mullo_epi32(c1_unpacked, c2_unpacked);
    result = _mm_srli_epi32(result, 8);

// Pack back to bytes
    result = _mm_packus_epi32(result, result);
    result = _mm_packus_epi16(result, result);
    return _mm_cvtsi128_si32(result);
}

/**
    \brief Linearly interpolate between two packed ARGB colors
    \param color1 packed 8-bit ARGB color (t = 0)
    \param color2 packed 8-bit ARGB color (t = 256)
    \param t Q8.8 interpolation factor (0 .. 256 = 0.0 .. 1.0)
    \return interpolated packed ARGB color
*/
inline uint32_t colorsLinearSSE4(uint32_t color1, uint32_t color2, uint16_t t)
{
// Load colors and unpack bytes to words
    __m128i c1 = _mm_cvtsi32_si128((int) color1);
    __m128i c2 = _mm_cvtsi32_si128((int) color2);
    __m128i c1_unpacked = _mm_cvtepu8_epi32(c1);
    __m128i c2_unpacked = _mm_cvtepu8_epi32(c2);

// Calculate difference (color2 - color1)
    __m128i diff = _mm_sub_epi32(c2_unpacked, c1_unpacked);

// Compute linear interpolation color1 + (color2 - color1) * t
    __m128i scaled_diff = _mm_mullo_epi32(diff, _mm_set1_epi32(t));
    __m128i result = _mm_add_epi32(c1_unpacked, _mm_srai_epi32(scaled_diff, 8));

// Pack back to bytes
    result = _mm_packus_epi32(result, result);
    result = _mm_packus_epi16(result, result);
    return _mm_cvtsi128_si32(result);
}

/**
    \brief Scale a packed ARGB color and accumulate it on another one
    \param color1 packed 8-bit ARGB color (accumulator)
    \param color2 packed 8-bit ARGB color (to be scaled)
    \param scale Q8.8 scale factor (0 .. 256 = 0.0 .. 1.0)
    \return color1 + color2 * scale, packed ARGB color (saturated)
*/
inline uint32_t colorsScaleAccumulateSSE4(uint32_t color1, uint32_t color2, uint16_t scale)
{
// Load colors and unpack bytes to words
    __m128i c1 = _mm_cvtsi32_si128((int) color1);
    __m128i c2 = _mm_cvtsi32_si128((int) color2);
    __m128i c1_unpacked = _mm_cvtepu8_epi32(c1);
    __m128i c2_unpacked = _mm_cvtepu8_epi32(c2);

// Compute scaling color2 * t
    __m128i scaled_c2 = _mm_mullo_epi32(c2_unpacked, _mm_set1_epi32(scale));
    scaled_c2 = _mm_srli_epi32(scaled_c2, 8);

// Add to accumulator (packus saturates to 255)
    __m128i result = _mm_add_epi32(c1_unpacked, scaled_c2);

// Pack back to bytes
    result = _mm_packus_epi32(result, result);
    result = _mm_packus_epi16(result, result);
    return _mm_cvtsi128_si32(result);
}

/*****************************************************************************/
/**
    \brief Linearly interpolate between two color vectors
    \param color1 4-float color vector (t = 0.0)
    \param color2 4-float color vector (t = 1.0)
    \param t interpolation factor (0.0 .. 1.0)
    \return interpolated color vector
*/
inline __m128 vectorLinearSSE4(__m128 color1, __m128 color2, float t)
{
    __m128 delta = _mm_sub_ps(color2, color1);
    return _mm_add_ps(color1, _mm_mul_ps(delta, _mm_set1_ps(t)));
}

/*****************************************************************************/
/**
    \brief Unpack a packed ARGB color into a normalised 4-float vector
    \param color packed 8-bit ARGB color
    \return color vector (components in 0.0 .. ~1.0, scaled by 1/256)
*/
inline __m128 unpackColorToVectorSSE4(uint32_t color)
{
    static constexpr float d8 = 1.0f / 256.0f;
    __m128i vec4i = _mm_cvtsi32_si128((int) color);
    vec4i = _mm_cvtepu8_epi32(vec4i);
    __m128  vec4f = _mm_cvtepi32_ps(vec4i);
    return _mm_mul_ps(vec4f, _mm_set1_ps(d8));
}

/**
    \brief Unpack a packed ARGB color into a scaled 4-float vector
    \param color packed 8-bit ARGB color
    \param scale scale factor applied to all components
    \return color vector (components in 0.0 .. ~scale)
*/
inline __m128 unpackColorToVectorScaledSSE4(uint32_t color, float scale)
{
    static constexpr float d8 = 1.0f / 256.0f;
    __m128i vec4i = _mm_cvtsi32_si128((int) color);
    vec4i = _mm_cvtepu8_epi32(vec4i);
    __m128  vec4f = _mm_cvtepi32_ps(vec4i);
    return _mm_mul_ps(vec4f, _mm_set1_ps(scale * d8));
}

/**
    \brief Pack a normalised 4-float color vector into a packed ARGB color
    \param color color vector (components in 0.0 .. 1.0)
    \return packed 8-bit ARGB color (saturated)
*/
inline uint32_t packVectorToColorSSE4(__m128 color)
{
    color = _mm_mul_ps(color, _mm_set1_ps(256.0f));
    __m128i vec4i = _mm_cvttps_epi32(color);
    vec4i = _mm_packus_epi32(vec4i, vec4i);
    vec4i = _mm_packus_epi16(vec4i, vec4i);
    return _mm_cvtsi128_si32(vec4i);
}

#endif // COLORS_H
