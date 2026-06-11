/**
    DIE ENGINE
    Depth Integration Engine / A modern ray-caster
    (c) Fred's Lab 2024-2026
    Frédéric Meslin / info@fredslab.net
    SPDX-License-Identifier: MIT
    If used commercially, contributions, donations are highly appreciated.

    postfx functions
*/

#ifndef POSTFX_H
#define POSTFX_H

#include <smmintrin.h>

#include <stdint.h>

/*****************************************************************************/
/**
    \brief Blend the previous frame into the current one (motion blur)
    \param frame current frame, receives the blended result
    \param previous previous frame, overwritten with the blended result
    \param blend Q8 blend factor (0 .. 256 = no blur .. full previous frame)
    \param size number of pixels (the trailing size % 4 pixels are untouched)
*/
inline void motionBlurSSE4(uint32_t * frame, uint32_t * previous, uint16_t blend, size_t size)
{
// Halved factor: keeps delta * blend within the signed 16-bit lanes
    __m128i blend1 = _mm_set1_epi16(blend >> 1);

    for (size_t i = 0; i + 4 <= size; i += 4) {
    // Load current (accumulated) and previous frames
        __m128i curr = _mm_loadu_si128((__m128i*) &frame[i]);
        __m128i prev = _mm_loadu_si128((__m128i*) &previous[i]);

    // Unpack to 16-bit channels
        __m128i curr_lo = _mm_unpacklo_epi8(curr, _mm_setzero_si128());
        __m128i curr_hi = _mm_unpackhi_epi8(curr, _mm_setzero_si128());
        __m128i prev_lo = _mm_unpacklo_epi8(prev, _mm_setzero_si128());
        __m128i prev_hi = _mm_unpackhi_epi8(prev, _mm_setzero_si128());

    // delta = (prev - curr)
        __m128i delta_lo = _mm_sub_epi16(prev_lo, curr_lo);
        __m128i delta_hi = _mm_sub_epi16(prev_hi, curr_hi);

    // weighted delta
        __m128i delta_lo_w = _mm_mullo_epi16(delta_lo, blend1);
        __m128i delta_hi_w = _mm_mullo_epi16(delta_hi, blend1);

    // result = curr + ((prev - curr) * b1) >> 7
        __m128i blended_lo = _mm_add_epi16(curr_lo, _mm_srai_epi16(delta_lo_w, 7));
        __m128i blended_hi = _mm_add_epi16(curr_hi, _mm_srai_epi16(delta_hi_w, 7));

    // Pack and store back to both frames
        __m128i result = _mm_packus_epi16(blended_lo, blended_hi);
        _mm_storeu_si128((__m128i*) &frame[i], result);
        _mm_storeu_si128((__m128i*) &previous[i], result);
    }
}

/*****************************************************************************/
/**
    \brief Fade the frame towards a solid color
    \param frame frame to fade, in place
    \param size number of pixels (the trailing size % 4 pixels are untouched)
    \param color packed ARGB target color
    \param factor Q8 fade amount (0 .. 256 = no change .. full color)
*/
inline void fadeToSSE4(uint32_t * frame, size_t size, uint32_t color, uint16_t factor)
{
    __m128i c = _mm_set1_epi32(color);

// Unpack color to 16-bit lanes (B,G,R,A)
    __m128i c_lo = _mm_unpacklo_epi8(c, _mm_setzero_si128());
    __m128i c_hi = _mm_unpackhi_epi8(c, _mm_setzero_si128());

// Halved factor: keeps (src - color) * f within the signed 16-bit lanes
    __m128i f = _mm_set1_epi16((short) ((256 - factor) >> 1));

    for (size_t i = 0; i + 4 <= size; i += 4) {
        __m128i pix = _mm_loadu_si128((__m128i *) &frame[i]);

    // Unpack pixels to 16-bit lanes
        __m128i lo = _mm_unpacklo_epi8(pix, _mm_setzero_si128());
        __m128i hi = _mm_unpackhi_epi8(pix, _mm_setzero_si128());

    // (src - color) * f
        lo = _mm_mullo_epi16(_mm_sub_epi16(lo, c_lo), f);
        hi = _mm_mullo_epi16(_mm_sub_epi16(hi, c_hi), f);

    // Divide by 128 (>> 7, halved factor)
        lo = _mm_srai_epi16(lo, 7);
        hi = _mm_srai_epi16(hi, 7);

    // Add color back
        lo = _mm_add_epi16(lo, c_lo);
        hi = _mm_add_epi16(hi, c_hi);

    // Pack back to 8-bit
        __m128i blended = _mm_packus_epi16(lo, hi);
        _mm_storeu_si128((__m128i*) &frame[i], blended);
    }
}

/*****************************************************************************/
/**
    \brief Scale 4 scattered pixels by a common factor
    \param factor Q8 darkening factor (0 .. 256 = black .. unchanged)
*/
inline void darken4SSE4(uint32_t * p0, uint32_t * p1, uint32_t * p2, uint32_t * p3, uint16_t factor)
{
// Load 4 uint32_t pixels into 128-bit register
    __m128i pixels = _mm_set_epi32(*p3, *p2, *p1, *p0);

// Convert to 4x (A,R,G,B) in 16-bit lanes
    __m128i lo = _mm_unpacklo_epi8(pixels, _mm_setzero_si128()); // pixels 0 + 1
    __m128i hi = _mm_unpackhi_epi8(pixels, _mm_setzero_si128()); // pixels 2 + 3

// Multiply by factor
    __m128i f = _mm_set1_epi16((short)factor);
    lo = _mm_mullo_epi16(lo, f);
    hi = _mm_mullo_epi16(hi, f);

// Divide by 256 (>> 8) — logical shift: product is unsigned (channel 0-255 * factor 0-256)
    lo = _mm_srli_epi16(lo, 8);
    hi = _mm_srli_epi16(hi, 8);

// Store back
    __m128i result = _mm_packus_epi16(lo, hi);
    *p0 = _mm_extract_epi32(result, 0);
    *p1 = _mm_extract_epi32(result, 1);
    *p2 = _mm_extract_epi32(result, 2);
    *p3 = _mm_extract_epi32(result, 3);
}

/**
    \brief Apply a circular vignette (darkened corners)
    \param inner_radius radius where the darkening starts, in pixels
    \param outer_radius radius where the frame turns black, in pixels
*/
inline void vignetteSSE4(uint32_t * frame, size_t width, size_t height, int inner_radius, int outer_radius)
{
    int w = (int) width;
    int h = (int) height;
    int cx = w >> 1;
    int cy = h >> 1;

    int inner2 = inner_radius * inner_radius;
    int outer2 = outer_radius * outer_radius;
    int range2 = outer2 - inner2;
    if (range2 <= 0) return;

    for (int y = 0; y < h / 2; y++) {
        int dy = y - cy;
        int dy2 = dy * dy;
        int ym = h - 1 - y;

        for (int x = 0; x < w / 2; x++) {
            int dx = x - cx;
            int dist2 = dx * dx + dy2;
            if (dist2 <= inner2) continue;

            int num = outer2 - dist2;
            if (num < 0) num = 0;
            uint16_t factor = (uint16_t) ((num << 8) / range2);

        // 4 symmetric pixels
            int xm = w - 1 - x;
            size_t i1 = (size_t) y * width + x;
            size_t i2 = (size_t) y * width + xm;
            size_t i3 = (size_t) ym * width + x;
            size_t i4 = (size_t) ym * width + xm;
            darken4SSE4(&frame[i1], &frame[i2], &frame[i3], &frame[i4], factor);
        }
    }
}

/*****************************************************************************/
/**
    \brief Apply a per-channel smoothstep tone curve
    \param ks per-channel gains {kr, kg, kb} (1.0 = full smoothstep)
*/
// ks = {kr, kg, kb}; applies per-channel smoothstep: q = c*k, c = 3q² - 2q³
inline void gammaSSE4(uint32_t * frame, size_t width, size_t height, float ks[3])
{
    const __m128 vkr     = _mm_set1_ps(ks[0]);
    const __m128 vkg     = _mm_set1_ps(ks[1]);
    const __m128 vkb     = _mm_set1_ps(ks[2]);
    const __m128 vinv255 = _mm_set1_ps(1.0f / 255.0f);
    const __m128 v255    = _mm_set1_ps(255.0f);
    const __m128 v3      = _mm_set1_ps(3.0f);

// Gather: collect one channel from the 4 BGRA pixels into low 4 bytes
    const __m128i gather_r  = _mm_set_epi8(-1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, 14,10, 6, 2);
    const __m128i gather_g  = _mm_set_epi8(-1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, 13, 9, 5, 1);
    const __m128i gather_b  = _mm_set_epi8(-1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, 12, 8, 4, 0);
// Scatter: spread each channel's 4 result bytes back to their pixel positions
    const __m128i scatter_r = _mm_set_epi8(-1, 3,-1,-1, -1, 2,-1,-1, -1, 1,-1,-1, -1, 0,-1,-1);
    const __m128i scatter_g = _mm_set_epi8(-1,-1, 3,-1, -1,-1, 2,-1, -1,-1, 1,-1, -1,-1, 0,-1);
    const __m128i scatter_b = _mm_set_epi8(-1,-1,-1, 3, -1,-1,-1, 2, -1,-1,-1, 1, -1,-1,-1, 0);
    const __m128i alpha_mask = _mm_set1_epi32((int)0xFF000000);

    const size_t size = width * height;
    for (size_t i = 0; i + 4 <= size; i += 4) {
        __m128i pixels = _mm_loadu_si128((__m128i*)&frame[i]);

    // Red: gather → float → smoothstep → pack
        __m128 qr = _mm_mul_ps(_mm_mul_ps(_mm_cvtepi32_ps(_mm_cvtepu8_epi32(
                        _mm_shuffle_epi8(pixels, gather_r))), vinv255), vkr);
        __m128 rf = _mm_mul_ps(_mm_mul_ps(qr, qr), _mm_sub_ps(v3, _mm_add_ps(qr, qr)));
        __m128i ri = _mm_packus_epi16(_mm_packus_epi32(
                        _mm_cvtps_epi32(_mm_mul_ps(rf, v255)), _mm_setzero_si128()),
                        _mm_setzero_si128());

    // Green
        __m128 qg = _mm_mul_ps(_mm_mul_ps(_mm_cvtepi32_ps(_mm_cvtepu8_epi32(
                        _mm_shuffle_epi8(pixels, gather_g))), vinv255), vkg);
        __m128 gf = _mm_mul_ps(_mm_mul_ps(qg, qg), _mm_sub_ps(v3, _mm_add_ps(qg, qg)));
        __m128i gi = _mm_packus_epi16(_mm_packus_epi32(
                        _mm_cvtps_epi32(_mm_mul_ps(gf, v255)), _mm_setzero_si128()),
                        _mm_setzero_si128());

    // Blue
        __m128 qb = _mm_mul_ps(_mm_mul_ps(_mm_cvtepi32_ps(_mm_cvtepu8_epi32(
                        _mm_shuffle_epi8(pixels, gather_b))), vinv255), vkb);
        __m128 bf = _mm_mul_ps(_mm_mul_ps(qb, qb), _mm_sub_ps(v3, _mm_add_ps(qb, qb)));
        __m128i bi = _mm_packus_epi16(_mm_packus_epi32(
                        _mm_cvtps_epi32(_mm_mul_ps(bf, v255)), _mm_setzero_si128()),
                        _mm_setzero_si128());

    // Reassemble: preserve alpha, scatter R/G/B back to BGRA positions
        __m128i out = _mm_and_si128(pixels, alpha_mask);
        out = _mm_or_si128(out, _mm_shuffle_epi8(ri, scatter_r));
        out = _mm_or_si128(out, _mm_shuffle_epi8(gi, scatter_g));
        out = _mm_or_si128(out, _mm_shuffle_epi8(bi, scatter_b));
        _mm_storeu_si128((__m128i*)&frame[i], out);
    }
}

#endif // POSTFX_H
