/**
    DIE ENGINE
    Depth Integration Engine / A modern ray-caster
    (c) Fred's Lab 2024-2026
    Frédéric Meslin / info@fredslab.net
    SPDX-License-Identifier: MIT
    If used commercially, contributions, donations are highly appreciated.

    2d engine
*/

#include "drawer.h"
#include "primitives.h"
#include "colors.h"

#include <QImage>

#include <stdlib.h>
#include <stdint.h>
#include <math.h>

/*****************************************************************************/
static void drawTriangle(QImage & image, const Triangle * triangle, const Texture & texture);

/*****************************************************************************/
void drawerQuad(QImage & image, const Quad * quad, const Texture & texture)
{
    Triangle tris[2] = {
        {
            {quad->xs[0], quad->xs[1], quad->xs[2]},
            {quad->ys[0], quad->ys[1], quad->ys[2]},
            {quad->us[0], quad->us[1], quad->us[2]},
            {quad->vs[0], quad->vs[1], quad->vs[2]},
            quad->surfaceId,
        },{
            {quad->xs[0], quad->xs[2], quad->xs[3]},
            {quad->ys[0], quad->ys[2], quad->ys[3]},
            {quad->us[0], quad->us[2], quad->us[3]},
            {quad->vs[0], quad->vs[2], quad->vs[3]},
            quad->surfaceId,
        }
    };

    drawerTriangle(image, &tris[0], texture);
    drawerTriangle(image, &tris[1], texture);
}

/*****************************************************************************/
void drawerTriangle(QImage & image, const Triangle * triangle, const Texture & texture)
{
    int low, mid, high;

//  Sort the 3 vertices by ascending y: low <= mid <= high
    if (triangle->ys[0] < triangle->ys[1]) {low = 0; high = 1;}
    else {low = 1; high = 0;}
    if (triangle->ys[2] < triangle->ys[low]) {mid = low; low = 2;}
    else {mid = 2;}

    if (triangle->ys[mid] > triangle->ys[high]) {
        int tmp = mid; mid = high; high = tmp;
    }

//  Reject degenerate (zero-height) triangles
    float dy = triangle->ys[high] - triangle->ys[low];
    if ((int) dy <= 0) return;

//  Split into a flat-bottom and a flat-top triangle, joined at the mid
//  scanline. When the triangle already has a flat top or bottom edge a
//  single triangle is enough. Vertex 0 of every emitted triangle is its
//  apex (the lone vertex), so drawTriangle never has to sort.
    Triangle tris[2];
    int triCount = 1;

    int topH = (int) triangle->ys[mid] - (int) triangle->ys[low];
    int botH = (int) triangle->ys[high] - (int) triangle->ys[mid];

    if (topH == 0) {
    //  Already flat-topped: apex is the high vertex
        tris[0] = {
            {triangle->xs[high], triangle->xs[low], triangle->xs[mid]},
            {triangle->ys[high], triangle->ys[low], triangle->ys[mid]},
            {triangle->us[high], triangle->us[low], triangle->us[mid]},
            {triangle->vs[high], triangle->vs[low], triangle->vs[mid]},
            triangle->surfaceId,
        };

    } else if (botH == 0) {
    //  Already flat-bottomed: apex is the low vertex
        tris[0] = {
            {triangle->xs[low], triangle->xs[mid], triangle->xs[high]},
            {triangle->ys[low], triangle->ys[mid], triangle->ys[high]},
            {triangle->us[low], triangle->us[mid], triangle->us[high]},
            {triangle->vs[low], triangle->vs[mid], triangle->vs[high]},
            triangle->surfaceId,
        };

    } else {
    //  Interpolate the split vertex 'e' on the long low -> high edge,
    //  at the mid scanline (k is the position of mid along that edge).
        float k  = (triangle->ys[mid] - triangle->ys[low]) / dy;
        float xe = triangle->xs[low] + (triangle->xs[high] - triangle->xs[low]) * k;
        float ye = triangle->ys[low] + (triangle->ys[high] - triangle->ys[low]) * k;
        float ue = triangle->us[low] + (triangle->us[high] - triangle->us[low]) * k;
        float ve = triangle->vs[low] + (triangle->vs[high] - triangle->vs[low]) * k;

    //  Upper flat-bottom triangle, apex = low, base = (mid, e)
        tris[0] = {
            {triangle->xs[low], triangle->xs[mid], xe},
            {triangle->ys[low], triangle->ys[mid], ye},
            {triangle->us[low], triangle->us[mid], ue},
            {triangle->vs[low], triangle->vs[mid], ve},
            triangle->surfaceId,
        };
    //  Lower flat-top triangle, apex = high, base = (mid, e)
        tris[1] = {
            {triangle->xs[high], triangle->xs[mid], xe},
            {triangle->ys[high], triangle->ys[mid], ye},
            {triangle->us[high], triangle->us[mid], ue},
            {triangle->vs[high], triangle->vs[mid], ve},
            triangle->surfaceId,
        };
        triCount++;
    }

    drawTriangle(image, &tris[0], texture);
    if (triCount == 1) return;
    drawTriangle(image, &tris[1], texture);
}

/*****************************************************************************/
void drawTriangle(QImage & image, const Triangle * triangle, const Texture & texture)
{
    uint32_t * frame = (uint32_t *) image.bits();
    uint32_t * tex   = texture.pixels + texture.block * triangle->surfaceId;

    const int width  = image.width();
    const int height = image.height();
    const int stride = image.bytesPerLine() / 4;
    const int mask   = texture.mask;
    const int size   = texture.size;

//  Vertex 0 is the apex, vertices 1 and 2 form the horizontal base
    float ax = triangle->xs[0], ay = triangle->ys[0];

    float sz = (float) size;
    float au = triangle->us[0] * sz, av = triangle->vs[0] * sz;

    float fullDy = triangle->ys[1] - ay;
    if (fullDy == 0.0f) return;
    float invDy = 1.0f / fullDy;

//  Per-scanline increments along both apex -> base edges (DDA): computed
//  once, then accumulated each row instead of re-interpolating from scratch
    float xaInc = (triangle->xs[1] - ax) * invDy;
    float uaInc = (triangle->us[1] * sz - au) * invDy;
    float vaInc = (triangle->vs[1] * sz - av) * invDy;
    float xbInc = (triangle->xs[2] - ax) * invDy;
    float ubInc = (triangle->us[2] * sz - au) * invDy;
    float vbInc = (triangle->vs[2] * sz - av) * invDy;

//  Walk every scanline covered by the triangle (apex and base, top first)
    float ybase = triangle->ys[1];
    int yStart = (int) ceilf(ay < ybase ? ay : ybase);
    int yStop  = (int) ceilf(ay < ybase ? ybase : ay);
    if (yStart < 0) yStart = 0;
    if (yStop > height) yStop = height;

//  Seed the edge accumulators at the first (top-clipped) scanline
    float prestep = (float) yStart - ay;
    float xa = ax + xaInc * prestep, ua = au + uaInc * prestep, va = av + vaInc * prestep;
    float xb = ax + xbInc * prestep, ub = au + ubInc * prestep, vb = av + vbInc * prestep;

    for (int y = yStart; y < yStop; y++) {
    //  Span runs from edge a to edge b; the drawing direction does not matter
    //  so the u/v slope is anchored at edge a and the bounds just use min/max
        float dx = xb - xa;
        if (dx != 0.0f) {
            float invDx = 1.0f / dx;
            float uInc = (ub - ua) * invDx;
            float vInc = (vb - va) * invDx;

            int xStart = (int) ceilf(xa < xb ? xa : xb);
            int xStop  = (int) ceilf(xa < xb ? xb : xa);
            if (xStart < 0) xStart = 0;
            if (xStop > width) xStop = width;

        //  Seed u/v on the span line at the first pixel (anchored at edge a)
            float preX = (float) xStart - xa;
            float uFloat = ua + uInc * preX;
            float vFloat = va + vInc * preX;

            uint32_t * row = frame + y * stride;

            for (int x = xStart; x < xStop; x++) {
            //  Bilinear texture fetch, wrapping on the power-of-two texture mask
                int u = (int) uFloat;
                int v = (int) vFloat;
                uint16_t uFrac = (uint16_t) ((uFloat - u) * 256.0f);
                uint16_t vFrac = (uint16_t) ((vFloat - v) * 256.0f);

                uint32_t src00 = tex[(u & mask) * size + (v & mask)];
                uint32_t src01 = tex[((u + 1) & mask) * size + (v & mask)];
                uint32_t src10 = tex[(u & mask) * size + ((v + 1) & mask)];
                uint32_t src11 = tex[((u + 1) & mask) * size + ((v + 1) & mask)];

                uint32_t src0 = colorsLinearSSE4(src00, src01, uFrac);
                uint32_t src1 = colorsLinearSSE4(src10, src11, uFrac);
                uint32_t src  = colorsLinearSSE4(src0, src1, vFrac);

            //  Blend over the frame, skipping fully transparent texels
                if (src >= 0x01000000) row[x] = colorsAlphaBlendSSE4(row[x], src);

                uFloat += uInc;
                vFloat += vInc;
            }
        }

    //  Step both edges down to the next scanline
        xa += xaInc; ua += uaInc; va += vaInc;
        xb += xbInc; ub += ubInc; vb += vbInc;
    }
}


