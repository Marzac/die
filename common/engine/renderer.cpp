/**
    DIE ENGINE
    Depth Integration Engine / A modern ray-caster
    (c) Fred's Lab 2024-2026
    Frédéric Meslin / info@fredslab.net
    SPDX-License-Identifier: MIT
    If used commercially, contributions, donations are highly appreciated.

    3d engine
*/

#include "renderer.h"
#include "colors.h"
#include "postfx.h"
#include "primitives.h"
#include "workerpool.h"

#include <QImage>
#include <QList>

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <math.h>
#include <string.h>
#include <type_traits>
#ifdef _MSC_VER
#include <intrin.h>
#endif

/*****************************************************************************/
Renderer renderer;
WorkerPool renderWorkers;

constexpr float epsilon = 0x1p-6f;

static const __m128 fullBlack = unpackColorToVectorSSE4(0x00000000);
static const __m128 unitWhite = _mm_setr_ps(1.0f, 1.0f, 1.0f, 0.0f);   // exact 1.0 gain (alpha lane cleared)

/*****************************************************************************/
/// \brief SSE flooring
static inline int floorToInt(float x)
{
    __m128 v = _mm_set_ss(x);
    return _mm_cvtss_si32(_mm_floor_ss(v, v));
}

/// \brief Index of the leftmost set bit, x must not be 0
static inline int highestBit(uint32_t x)
{
#ifdef _MSC_VER
    unsigned long i;
    _BitScanReverse(&i, x);
    return (int) i;
#else
    return 31 - __builtin_clz(x);
#endif
}

/*****************************************************************************/
/**
    \brief Per-surface lighting terms, from the SURFACE_FLAGS
    The pixel is lit as: lit = texel * (env + glowmap) + (bleedEnv + glowmap)^2 * bleed
    The square only burns the brightly lit spots; bleedEnv is the env light with FLAG_SUN_BLEED, black otherwise
*/
struct SurfaceLighting {
    __m128 env;         ///< environment light (sun ambient + ray), unit gain when fullbright
    __m128 bleed;       ///< bleed factor, zero when disabled
    __m128 bleedEnv;    ///< env light part that bleeds, black unless FLAG_SUN_BLEED
    bool glow;          ///< sample the glowmap (local / dynamic lights)
    bool occlusion;     ///< ambient occlusion allowed
};

static inline SurfaceLighting surfaceLighting(uint16_t surfaceFlags, __m128 ambient, __m128 ray, __m128 bleed, bool sunBleed)
{
    bool noAmbient = surfaceFlags & SURFACE_FLAG_NO_AMBIENT;
    bool noRay = surfaceFlags & SURFACE_FLAG_NO_RAY;
    bool noGlow = surfaceFlags & SURFACE_FLAG_NO_GLOW;

// Fullbright: the texels as they are
    if (noAmbient && noRay && noGlow) return {unitWhite, fullBlack, fullBlack, false, false};

    SurfaceLighting l;
    l.env = _mm_add_ps(noAmbient ? fullBlack : ambient, noRay ? fullBlack : ray);
    l.bleed = (surfaceFlags & SURFACE_FLAG_NO_BLEED) ? fullBlack : bleed;
    l.bleedEnv = sunBleed ? l.env : fullBlack;
    l.glow = !noGlow;
    l.occlusion = !(surfaceFlags & SURFACE_FLAG_NO_AO);
    return l;
}

/*****************************************************************************/
Renderer::Renderer() :
    flags(FLAGS_DEFAULT),
    nodes(nullptr), nodesCount(0), nodesAllocated(0),
    walls(nullptr), wallsCount(0), wallsAllocated(0),
    textures(nullptr), texturesCount(0), texturesAllocated(0),
    lights(nullptr), lightsCount(0), lightsAllocated(0),
    pendingFrameResoX(0), pendingFrameResoY(0), pendingGlowmapReso(0),
    image(nullptr), frame(nullptr), frameLast(nullptr),
    framesAllocated(false),
    segments(nullptr),
    glowmap(nullptr), glowmapStill(nullptr),
    panTable(nullptr)
{
}

/*****************************************************************************/
void Renderer::init()
{
    configInit();
    configLoad("die.ini");

    allocateFrames(frameResoX, frameResoY);
    allocateGlowmap(glowmapReso);
    allocateObjects(nodesAllocated, wallsAllocated, texturesAllocated, lightsAllocated);

    clickX = clickY = 0;
    clickRegistered = false;
    clickWallID = ClickNoWall;

    viewPoint = {};
    sceneCornerBegin = sceneCornerEnd = QVector2D(0.0f, 0.0f);

    floorRay = 0x00FFFFFF;
    ceilingRay = 0x00FFFFFF;

    configSave("die.ini");
}

void Renderer::terminate()
{
    configSave("die.ini");
    deallocate();
}

/*****************************************************************************/
void Renderer::setFlags(FLAGS flags)
{
    this->flags = flags;
}

void Renderer::checkFlag(FLAGS flag, bool checked)
{
    flags &= ~(uint32_t) flag;
    if (checked) flags |= flag;
}

/*****************************************************************************/
void Renderer::setGlowmapReso(int size)
{
    if (!size) return;
    pendingGlowmapReso = size;
}

void Renderer::setGlowmapArea(float area)
{
    if (area <= 0.0f) return;
    glowmapArea = area;
    flags |= Renderer::FLAG_GLOWMAP_REBUILD;    // new scale: the still lights move
}

void Renderer::setFramesReso(int width, int height)
{
    if (!width) return;
    if (!height) return;
    pendingFrameResoX = width;
    pendingFrameResoY = height;
}

void Renderer::setFOVAngle(float angle)
{
    fovAngle = angle;
    buildTables();
}

void Renderer::setFOVDistanceNear(float distance)
{
    fovDistanceNear = distance;
}

void Renderer::setFOVDistanceFar(float distance)
{
    fovDistanceFar = distance;
}

/*****************************************************************************/
void Renderer::allocateObjects(int noNodes, int noWalls, int noTextures, int noLights)
{
    nodesAllocated = 0;
    wallsAllocated = 0;
    texturesAllocated = 0;
    lightsAllocated = 0;

    nodesCount = 0;
    wallsCount = 0;
    texturesCount = 0;
    lightsCount = 0;

    delete[] nodes; nodes = nullptr;
    delete[] walls; walls = nullptr;
    delete[] segments; segments = nullptr;
    delete[] textures; textures = nullptr;
    delete[] lights; lights = nullptr;

    if (!(nodes = new (std::nothrow) Node[noNodes]())) return;
    nodesAllocated = noNodes;
    if (!(walls = new (std::nothrow) Wall[noWalls]())) return;
    if (!(segments = new (std::nothrow) Segment[noWalls])) return;
    wallsAllocated = noWalls;
    if (!(textures = new (std::nothrow) Texture[noTextures])) return;
    texturesAllocated = noTextures;
    if (!(lights = new (std::nothrow) Light[noLights])) return;
    lightsAllocated = noLights;

    memset(segments, 0, sizeof(Segment) * noWalls);
    memset(textures, 0, sizeof(Texture) * noTextures);
    memset(lights, 0, sizeof(Light) * noLights);
}

void Renderer::allocateFrames(int width, int height)
{
    framesAllocated = false;

    if (!allocatePixelBuffer(frameLast, width, height)) return;

    delete[] panTable;
    if (!(panTable = new (std::nothrow) float[width])) return;

    delete image;
    if (!(image = new (std::nothrow) QImage(width, height, QImage::Format_RGB32))) return;
    frame = (uint32_t *) image->bits();

    frameResoX = width;
    frameResoY = height;
    buildTables();

    framesAllocated = true;
}

void Renderer::allocateGlowmap(int size)
{
    glowmapReso = 0;
    delete[] glowmap;
    delete[] glowmapStill;
    glowmap = nullptr;
    glowmapStill = nullptr;
    if (!(glowmap = new (std::nothrow) __m128[size * size])) return;
    if (!(glowmapStill = new (std::nothrow) __m128[size * size])) {delete[] glowmap; glowmap = nullptr; return;}
// Both maps start black: the dynamic one is only rewritten inside the dirty boxes
    memset(glowmap, 0, size * size * sizeof(__m128));
    memset(glowmapStill, 0, size * size * sizeof(__m128));
    glowmapReso = size;
// The still lights must be drawn again into the new maps
    flags |= Renderer::FLAG_GLOWMAP_REBUILD;
}

bool Renderer::allocatePixelBuffer(uint32_t *& pixels, int width, int height)
{
    size_t size = width * height * sizeof(uint32_t);
    if (pixels) free(pixels);
    pixels = (uint32_t *) malloc(size);
    if (pixels) memset(pixels, 0, size);
    return pixels != nullptr;
}

void Renderer::deallocate()
{
    delete image; image = nullptr;

    if (frameLast) {free(frameLast); frameLast = nullptr;}

    delete[] panTable; panTable = nullptr;
    delete[] glowmap; glowmap = nullptr;
    delete[] glowmapStill; glowmapStill = nullptr;
    framesAllocated = false;
}

/*****************************************************************************/
void Renderer::clickRegister(int x, int y)
{
    clickX = x;
    clickY = y;
    clickRegistered = true;
    clickWallID = ClickNoWall;
}

/*****************************************************************************/
inline void Renderer::sceneGetCoords(const float x, const float z, float & xs, float & zs)
{
    xs = x * glowmapScale + glowmapReso * 0.5f;
    zs = z * glowmapScale + glowmapReso * 0.5f;
}

inline void Renderer::worldGetCoords(const float xs, const float zs, float & x, float & z)
{
    x = (xs - glowmapReso * 0.5f) * glowmapInvScale;
    z = (zs - glowmapReso * 0.5f) * glowmapInvScale;
}

/*****************************************************************************/
void Renderer::glowmapChunk(__m128 * gm, bool still, int z1, int z2)
{
// Recopy only the dirty regions
    if (!still) {
        for (const BoundingBox & r : glowmapDirtyBoxes) {
            int szMin = std::max(r.beginZ, z1);
            int szMax = std::min(r.endZ, z2);
            for (int sz = szMin; sz < szMax; sz++) {
                int row = sz * glowmapReso;
                memcpy(&gm[row + r.beginX], &glowmapStill[row + r.beginX], (r.endX - r.beginX) * sizeof(__m128));
            }
        }
    }

    for (int j = 0; j < lightsCount; j++) {
        const Light & l = lights[j];
        if (l.still != still) continue;

        int szMin = std::max(l.boundingBox.beginZ, z1);
        int szMax = std::min(l.boundingBox.endZ, z2);

        for (int sz = szMin; sz < szMax; sz++) {
            __m128 * row = &gm[sz * glowmapReso];
            for (int sx = l.boundingBox.beginX; sx < l.boundingBox.endX; sx++) {
                float x, z;
                worldGetCoords(sx, sz, x, z);
                float rdx = l.x - x;
                float rdz = l.z - z;

                bool masked = false;
                for (int i = 0; i < wallsCount; i++) {
                    const Wall & w = walls[i];
                    if (w.flags & WALL_FLAG_NO_SHADOW) continue;

                    const Segment & s = segments[i];
                    float div = rdx * s.deltaZ - rdz * s.deltaX;
                    if (fabs(div) < epsilon) continue;
                    if ((w.flags & WALL_FLAG_BACKCULLED) && div < 0.0f) continue;

                    float scale = 1.0f / div;
                    float t = ((s.beginX - x) * s.deltaZ - (s.beginZ - z) * s.deltaX) * scale;
                    if (t < 0.0f || t > 1.0f) continue;
                    float u = ((s.beginX - x) * rdz - (s.beginZ - z) * rdx) * scale;
                    if (u < 0.0f || u > 1.0f) continue;
                    masked = true;
                    break;
                }

                if (masked) continue;

                float d2 = rdx * rdx + rdz * rdz;
                float lf = 1.0f / (1.0f + GlowFalloffSharpness * d2 * l.falloffInv2);
                row[sx] = _mm_add_ps(row[sx], _mm_mul_ps(l.argb, _mm_set1_ps(lf)));
            }
        }
    }
}

/*****************************************************************************/
void Renderer::glowmapConcurrent(__m128 * gm, bool still)
{
    int threads = renderWorkers.workerCount();
    int band = glowmapReso / threads;

    for (int i = 0; i < threads; i++) {
        int z1 = i * band;
        int z2 = (i == threads - 1) ? glowmapReso : z1 + band;
        renderWorkers.enqueue([this, gm, still, z1, z2]() {
            glowmapChunk(gm, still, z1, z2);
        });
    }

    renderWorkers.waitAll();
}

/*****************************************************************************/
void Renderer::lightsMash()
{
// Sun rays on floors and ceilings
    uint8_t kFloor = (uint8_t)(std::clamp(sunRayDirection.y() * sunRayStrength, 0.0f, 1.0f) * 255.0f);
    floorRay = colorsScaleSSE4(sunRayColor, (uint16_t)kFloor << 8);
    uint8_t kCeil = (uint8_t)(std::clamp(-sunRayDirection.y() * sunRayStrength, 0.0f, 1.0f) * 255.0f);
    ceilingRay = colorsScaleSSE4(sunRayColor, (uint16_t)kCeil << 8);

// Prepare the lights
    sunAmbientArgb = unpackColorToVectorScaledSSE4(sunAmbient, sunAmbientStrength);
    sunRayColorArgb = unpackColorToVectorScaledSSE4(sunRayColor, sunRayStrength);
    QList<BoundingBox> dirtyBoxes;
    for (int j = 0; j < lightsCount; j++) {
    // Compute base color and falloff
        Light & l = lights[j];
        l.argb = unpackColorToVectorScaledSSE4(l.color, l.strength * l.strength);
        l.x = nodes[l.nodeID].pos.x();
        l.z = nodes[l.nodeID].pos.z();
        l.falloffInv2 = 1.0f / (l.falloff * l.falloff);

    // Calculate the bounding box of influence
        float radius = l.falloff;
        float sMinX, sMinZ, sMaxX, sMaxZ;
        sceneGetCoords(l.x - radius, l.z - radius, sMinX, sMinZ);
        sceneGetCoords(l.x + radius, l.z + radius, sMaxX, sMaxZ);
        l.boundingBox.beginX = std::clamp((int)sMinX, 0, glowmapReso);
        l.boundingBox.endX = std::clamp((int)sMaxX, 0, glowmapReso);
        l.boundingBox.beginZ = std::clamp((int)sMinZ, 0, glowmapReso);
        l.boundingBox.endZ = std::clamp((int)sMaxZ, 0, glowmapReso);

        if (!l.still && l.boundingBox.beginX < l.boundingBox.endX && l.boundingBox.beginZ < l.boundingBox.endZ)
            dirtyBoxes.append(l.boundingBox);
    }

// Compute the glowmaps
    glowBleed = _mm_set1_ps(GlowBleedFactor);
    const size_t gmSize = glowmapReso * glowmapReso * sizeof(__m128);
    if (!(flags & Renderer::FLAG_LIGHTS)) {
        memset(glowmap, 0, gmSize);
        glowmapDirtyLast = {{0, 0, glowmapReso, glowmapReso}};
        return;
    }

 //
    if (flags & Renderer::FLAG_GLOWMAP_REBUILD) {
        memset(glowmapStill, 0, gmSize);
        if (flags & Renderer::FLAG_MULTITHREADING)
            glowmapConcurrent(glowmapStill, true);
        else glowmapChunk(glowmapStill, true, 0, glowmapReso);
        flags &= ~Renderer::FLAG_GLOWMAP_REBUILD;
        dirtyBoxes.append({0, 0, glowmapReso, glowmapReso});    // {beginX, beginZ, endX, endZ}: the whole map
    }

// Concatenate the dirty boxes and keep the new one for next frame
    glowmapDirtyBoxes = glowmapDirtyLast + dirtyBoxes;
    glowmapDirtyLast = std::move(dirtyBoxes);

    if (flags & Renderer::FLAG_MULTITHREADING) glowmapConcurrent(glowmap, false);
    else glowmapChunk(glowmap, false, 0, glowmapReso);
}

/*****************************************************************************/
inline void Renderer::sceneBoundsInit()
{
    sceneCornerBegin = QVector2D(+32768.0f, +32768.0f);
    sceneCornerEnd = QVector2D(-32768.0f, -32768.0f);
}

inline void Renderer::sceneBoundsRegister(float x, float z)
{
    if (x < sceneCornerBegin.x()) sceneCornerBegin.setX(x);
    if (x > sceneCornerEnd.x()) sceneCornerEnd.setX(x);
    if (z < sceneCornerBegin.y()) sceneCornerBegin.setY(z);
    if (z > sceneCornerEnd.y()) sceneCornerEnd.setY(z);
}

/*****************************************************************************/
void Renderer::wallsMash()
{
    QVector2D dir(sunRayDirection.x(), sunRayDirection.z());
    sceneCornerBegin = sceneCornerEnd = QVector2D(0.0f, 0.0f);

    for (int i = 0; i < wallsCount; i++) {
        Wall & w = walls[i];
        Segment & s = segments[i];

    // Cache coordinates
        s.beginX = nodes[w.nodeID1].pos.x();
        s.beginZ = nodes[w.nodeID1].pos.z();
        sceneBoundsRegister(s.beginX, s.beginZ);
        s.endX = nodes[w.nodeID2].pos.x();
        s.endZ = nodes[w.nodeID2].pos.z();
        sceneBoundsRegister(s.endX, s.endZ);

    // Prepare the segment
        float ex = nodes[w.nodeID2].pos.x();
        float ez = nodes[w.nodeID2].pos.z();
        s.deltaX = ex - s.beginX;
        s.deltaZ = ez - s.beginZ;
        s.length = sqrtf(s.deltaX * s.deltaX + s.deltaZ * s.deltaZ);
        if (s.length == 0.0f) {w.flags = WALL_FLAG_FREE; continue;}
        s.invLen = 1.0f / s.length;

    // Compute lighting
        QVector2D normal(s.deltaZ * s.invLen, -s.deltaX * s.invLen);
        float ps = QVector2D::dotProduct(normal, dir);
        uint8_t f = (uint8_t) (std::clamp(ps * sunRayStrength, 0.0f, 1.0f) * 255.0f);
        w.rayFront = colorsScaleSSE4(sunRayColor, (uint16_t)f << 8);
        uint8_t b = (uint8_t) (std::clamp(-ps * sunRayStrength, 0.0f, 1.0f) * 255.0f);
        w.rayBack = colorsScaleSSE4(sunRayColor, (uint16_t)b << 8);
    }

// Glow map scale: fixed area centered at origin
    glowmapScale = glowmapReso / glowmapArea;
    glowmapInvScale = 1.0f / glowmapScale;
}

/*****************************************************************************/
void Renderer::fogSunMash()
{
    if (fogDistanceFar > fogDistanceNear) {
        fogDistanceInv = 1.0f / (fogDistanceFar - fogDistanceNear);
        fogDistanceOffset = fogDistanceFar;

    }else{
        fogDistanceInv = 0.0f;
        fogDistanceOffset = 0.0f;
    }
}

/*****************************************************************************/
void Renderer::render(Viewpoint & vp)
{
    int pw = pendingFrameResoX.exchange(0);
    int ph = pendingFrameResoY.exchange(0);
    if (pw && ph) allocateFrames(pw, ph);

    int pg = pendingGlowmapReso.exchange(0);
    if (pg) allocateGlowmap(pg);

    if (!framesAllocated) return;
    if (!nodesAllocated) return;
    if (!wallsAllocated) return;
    if (!texturesAllocated) return;
    if (!glowmapReso) return;

// Clear the frames
    memset(frame, 0, frameResoX * frameResoY * sizeof(uint32_t));

// Render pre-compute
    fogSunMash();
    wallsMash();
    lightsMash();

// Render frame
    viewPoint = vp;
    levels = probeLevels(viewPoint.pos + viewPoint.offset, viewPoint.pan);
    if (flags & Renderer::FLAG_MULTITHREADING) renderConcurrent();
    else renderChunk(renderStates[0], 0, frameResoX);

// Render post-fx
    if (flags & Renderer::FLAG_MOTIONBLUR) {
        uint16_t blend = motionBlurFactor * 256.0f * 0.01f;
        motionBlurSSE4(frame, frameLast, blend, frameResoX * frameResoY);
    }

    if (flags & Renderer::FLAG_VIGNETTE) {
        uint16_t inner = vignetteInnerRadius * frameResoY * 0.005f;
        uint16_t outer = vignetteOuterRadius * frameResoY * 0.005f;
        if (outer <= inner) outer = inner + 1;
        vignetteSSE4(frame, frameResoX, frameResoY, inner, outer);
    }

    if (flags & Renderer::FLAG_GAMMA) {
        float ks[3] = {gammaKRed, gammaKGreen, gammaKBlue};
        gammaSSE4(frame, frameResoX, frameResoY, ks);
    }
}

/*****************************************************************************/
void Renderer::renderConcurrent()
{
    int threads = renderWorkers.workerCount();
    int band = frameResoX / threads;

    for (int i = 0; i < threads; ++i) {
        int x1 = i * band;
        int x2 = (i == threads - 1) ? frameResoX : x1 + band;

        Context& state = renderStates[i];
        renderWorkers.enqueue([this, &state, x1, x2]() {
            renderChunk(state, x1, x2);
        });
    }

    renderWorkers.waitAll();
}

/*****************************************************************************/
void Renderer::renderChunk(Context & state, int x1, int x2)
{
    const float angle = fovAngle * D2R;

// Clamp the shift to half the frame height
    state.casterVShift = frameResoY * viewPoint.tilt / angle;
    state.casterVShift = std::clamp(state.casterVShift, -0.5f * frameResoY, 0.5f * frameResoY);
    state.casterHorizon = frameResoY * 0.5f + state.casterVShift;
    state.casterProjectionX = projectionX();
    state.casterProjectionY = projectionY();
    state.casterPos = viewPoint.pos + viewPoint.offset;

// Create the strips map
    for (int i = 0; i < StripsMax; i++)
        state.vStripsMap[i] = i;

// Render each column
    for (state.casterColumn = x1; state.casterColumn < x2; state.casterColumn++) {
        float pan = panTable[state.casterColumn];
        state.casterRayX = cosf(pan + viewPoint.pan);
        state.casterRayZ = sinf(pan + viewPoint.pan);
        state.casterPC = 1.0f / cosf(pan);
        castStrips(state);

    // Floor / ceiling planes
        Cap ceiling = {levels.ceiling.h, levels.ceiling.gx * state.casterRayX + levels.ceiling.gz * state.casterRayZ};
        Cap floor = {levels.floor.h, levels.floor.gx * state.casterRayX + levels.floor.gz * state.casterRayZ};

    // Render surfaces
        state.vStripsClippedCount = 0;
        CapStack upperBounds = {};
        upperBounds.push({HeightMax, 0.0f});
        upperBounds.push({ceiling.h0 + epsilon, ceiling.slope});
        CapStack lowerBounds = {};
        lowerBounds.push({HeightMin, 0.0f});
        lowerBounds.push({floor.h0 - epsilon, floor.slope});
        renderVertical(state, 0, upperBounds, lowerBounds, 0, frameResoY);

    // Render walls
        if (flags & Renderer::FLAG_WALLS) drawVStrips(state);
    }
}

/*****************************************************************************/
void Renderer::castStrips(Context & state)
{
    state.vStripsCount = 0;
    float casterX = state.casterPos.x();
    float casterY = state.casterPos.y();
    float casterZ = state.casterPos.z();
    float invRayX = state.casterRayX != 0.0f ? 1.0f / state.casterRayX : 32768.0f;
    float invRayZ = state.casterRayZ != 0.0f ? 1.0f / state.casterRayZ : 32768.0f;

    for (int i = 0; i < wallsCount; i++) {
        Segment & s = segments[i];
        float dist = 0.0f;
        float k = 0.0f;

    // Find intersection with wall
        if (state.casterRayX * state.casterRayX > state.casterRayZ * state.casterRayZ) {
            float ratio = state.casterRayZ * invRayX;
            float delta = s.deltaX * ratio - s.deltaZ;
            if (delta == 0.0f) continue;
            float kd = (s.beginZ - casterZ) - (s.beginX - casterX) * ratio;
            if (delta > 0.0f) {if (kd < 0.0f || kd > delta) continue;}
            else {if (kd > 0.0f || kd < delta) continue;}
            k = kd / delta;
            dist = ((s.beginX - casterX) + k * s.deltaX) * invRayX;

        }else{
            float ratio = state.casterRayX * invRayZ;
            float delta = s.deltaZ * ratio - s.deltaX;
            if (delta == 0.0f) continue;
            float kd = (s.beginX - casterX) - (s.beginZ - casterZ) * ratio;
            if (delta > 0.0f) {if (kd < 0.0f || kd > delta) continue;}
            else {if (kd > 0.0f || kd < delta) continue;}
            k = kd / delta;
            dist = ((s.beginZ - casterZ) + k * s.deltaZ) * invRayZ;
        }

    // Clipping the intersection
        if (dist < fovDistanceNear) continue;
        if (dist > fovDistanceFar) continue;

    // Find wall height
        Wall & w = walls[i];
        float rowsPerUnit = (state.casterPC * state.casterProjectionY) / dist;
        float bot1 = nodes[w.nodeID1].pos.y();
        float bot2 = nodes[w.nodeID2].pos.y();
        float bot = bot1 + (bot2 - bot1) * k;
        float top = bot + w.height;

    // Create a strip object
        Strip & strip = state.vStrips[state.vStripsCount];
        strip.k = k;
        strip.yTop = state.casterHorizon - (top - casterY) * rowsPerUnit;
        strip.yBot = state.casterHorizon - (bot - casterY) * rowsPerUnit;
        strip.yNoclipTop = strip.yTop;
        strip.yNoclipBot = strip.yBot;
        strip.yDarkenTop = strip.yTop + occlusionLength * rowsPerUnit;
        strip.yDarkenBot = strip.yBot - occlusionLength * rowsPerUnit;
        strip.hTop = top;
        strip.hBot = bot;
        strip.vTop = top - bot;
        strip.vBot = 0.0f;
        strip.u = k * s.length;
        strip.dist = dist;
        strip.wallId = i;
        strip.back = (state.casterRayX * s.deltaZ - state.casterRayZ * s.deltaX) > 0.0f;

    // Translate the wall flags into strip flags
        strip.flags = VSTRIP_FLAG_FREE;
        if (w.flags & WALL_FLAG_INVISIBLE) strip.flags |= VSTRIP_FLAG_INVISIBLE;
        if (w.flags & WALL_FLAG_HIGHLIGHTED) strip.flags |= VSTRIP_FLAG_HIGHLIGHTED;

        if (strip.back) {
            if (w.flags & WALL_FLAG_CEILING_BACK) strip.flags |= VSTRIP_FLAG_HASCEILING;
            if (w.flags & WALL_FLAG_FLOOR_BACK) strip.flags |= VSTRIP_FLAG_HASFLOOR;
        }else{
            if (w.flags & WALL_FLAG_CEILING_FRONT) strip.flags |= VSTRIP_FLAG_HASCEILING;
            if (w.flags & WALL_FLAG_FLOOR_FRONT) strip.flags |= VSTRIP_FLAG_HASFLOOR;
        }

        if (w.flags & WALL_FLAG_INVISIBLE)
            strip.flags |= VSTRIP_FLAG_SEETHROUGH;
        else if (w.surfaces[strip.back ? WALL_SURFACE_BACK : WALL_SURFACE_FRONT].flags & SURFACE_FLAG_ALPHA)
            strip.flags |= VSTRIP_FLAG_SEETHROUGH;

        if (strip.back && (w.flags & WALL_FLAG_BACKCULLED))
            strip.flags |= VSTRIP_FLAG_BACKCULLED | VSTRIP_FLAG_SEETHROUGH;

    // Prepare the sorting
        state.vStripsMap[state.vStripsCount] = state.vStripsCount;
        state.vStripsZOrders[state.vStripsCount++] = dist * dist;
    }

// Sort the strips per depth
    std::sort(state.vStripsMap, state.vStripsMap + state.vStripsCount,
        [&](uint16_t a, uint16_t b) {
        float d1 = state.vStripsZOrders[a];
        float d2 = state.vStripsZOrders[b];
        return d1 < d2;
    });
}

/*****************************************************************************/
Renderer::Levels Renderer::probeLevels(const QVector3D & pos, float pan)
{
    Context & state = probeState;
    state.casterPos = pos;
    state.casterRayX = cosf(pan);
    state.casterRayZ = sinf(pan);
    state.casterPC = 1.0f;
    state.casterProjectionX = 1.0f;
    state.casterProjectionY = 1.0f;
    state.casterHorizon = 0.0f;
    castStrips(state);

// Walk from the furthest wall to the closest, a wall hides the planes it crosses
    static constexpr int LevelsMax = 2 * StripsMax;
    Level found[LevelsMax];
    int foundCount = 0;
    for (int depth = state.vStripsCount - 1; depth >= 0; depth--) {
        const Strip & strip = state.vStrips[state.vStripsMap[depth]];
        for (int i = foundCount - 1; i >= 0; i--) {
            const Level & l = found[i];
            float h = l.h + strip.dist * (l.gx * state.casterRayX + l.gz * state.casterRayZ);
            if (h >= strip.hBot - epsilon && h <= strip.hTop + epsilon)
                found[i] = found[--foundCount];
        }
        if (strip.flags & VSTRIP_FLAG_HASCEILING) found[foundCount++] = capLevel(state, strip, strip.back, true);
        if (strip.flags & VSTRIP_FLAG_HASFLOOR) found[foundCount++] = capLevel(state, strip, strip.back, false);
    }

// Closest plane above = ceiling, below = floor
    Levels result = {{HeightMin, 0.0f, 0.0f}, {HeightMax, 0.0f, 0.0f}};
    float y = pos.y();
    for (int i = 0; i < foundCount; i++) {
        const Level & l = found[i];
        if (l.h > y && l.h < result.ceiling.h) result.ceiling = l;
        if (l.h < y && l.h > result.floor.h) result.floor = l;
    }
    return result;
}

/*****************************************************************************/
Renderer::Level Renderer::capLevel(const Context & state, const Strip & strip, bool back, bool ceiling)
{
    const Wall & w = walls[strip.wallId];
    const Segment & s = segments[strip.wallId];
    Cap cap = ceiling ? capCeilingHeight(state, strip, back) : capFloorHeight(state, strip, back);

// Plane gradient: rise along the wall, drop away from it on the surface side
    float rise = (nodes[w.nodeID2].pos.y() - nodes[w.nodeID1].pos.y()) * s.invLen;
    float drop = ceiling ? (back ? w.ceilingBackDrop : w.ceilingFrontDrop)
                         : (back ? w.floorBackDrop : w.floorFrontDrop);
    drop *= 0x1p-3f;
    float ux = s.deltaX * s.invLen;
    float uz = s.deltaZ * s.invLen;
    float nx = back ? -uz : uz;     // front normal is (uz, -ux)
    float nz = back ? ux : -ux;
    return {cap.h0, rise * ux - drop * nx, rise * uz - drop * nz};
}

/*****************************************************************************/
void Renderer::renderVertical(Context & state, int depth,
                            CapStack upperBounds, CapStack lowerBounds,
                            int scanTop, int scanBot)
{
    float camY = state.casterPos.y();
    for (int i = depth; i < state.vStripsCount; i++) {
        Strip & strip = state.vStrips[state.vStripsMap[i]];
        Wall & w = walls[strip.wallId];

    // Window bounds, compared at the wall distance
        float wBot = strip.hBot;
        float wTop = strip.hTop;
        Cap upperCap = upperBounds.top();
        Cap lowerCap = lowerBounds.top();
        float upperBound = upperCap.h0 + upperCap.slope * strip.dist;
        float lowerBound = lowerCap.h0 + lowerCap.slope * strip.dist;

    // Case 0: Clip walls
    // Edge on the bound: its plane ends here, hand over to the wall's own bound
        if (wTop < lowerBound) {
            if (lowerBounds.count > 1 && lowerBound - wTop <= 2.0f * epsilon) {
                Cap lower;
                if (edgeLowerBound(state, strip, lower)) lowerBounds.replace(lower);
                else lowerBounds.pop();
            }
            continue;
        }
        if (wBot > upperBound) {
            if (upperBounds.count > 1 && wBot - upperBound <= 2.0f * epsilon) {
                Cap upper;
                if (edgeUpperBound(state, strip, upper)) upperBounds.replace(upper);
                else upperBounds.pop();
            }
            continue;
        }

    // Case 1: Floor surface seen from below
        if ((strip.flags & VSTRIP_FLAG_HASFLOOR) &&
            camY < capFloorHeight(state, strip, strip.back).h0) {
            int bot = std::min((int)strip.yBot, scanBot);
            if (bot < scanTop) bot = scanTop;
            if (bot < 0) bot = 0;

        // A floor on the lower bound cannot be seen from below
            if (wBot < upperBound && wBot - lowerBound > 2.0f * epsilon) {
                if (flags & Renderer::FLAG_SURFACES)
                    vplaneDraw(state, strip, WALL_SURFACE_FLOOR, scanTop, bot);
            // Edge on the bound: hand over as in case 0
                if (upperBounds.count > 1 && upperBound - wBot <= 2.0f * epsilon) {
                    Cap upper;
                    if (edgeUpperBound(state, strip, upper)) upperBounds.replace(upper);
                    else upperBounds.pop();
                }
                renderVertical(state, i + 1, upperBounds, lowerBounds, bot, scanBot);
                return;
            }
        }

    // Case 2: Ceiling surface seen from above
        if ((strip.flags & VSTRIP_FLAG_HASCEILING) &&
            camY > capCeilingHeight(state, strip, strip.back).h0) {
            int top = std::max((int)strip.yTop, scanTop);
            if (top > scanBot) top = scanBot;
            if (top > frameResoY) top = frameResoY;

        // A ceiling on the upper bound cannot be seen from above
            if (wTop > lowerBound && upperBound - wTop > 2.0f * epsilon) {
                if (flags & Renderer::FLAG_SURFACES)
                    vplaneDraw(state, strip, WALL_SURFACE_CEILING, top, scanBot);
            // Edge on the bound: hand over as in case 0
                if (lowerBounds.count > 1 && wTop - lowerBound <= 2.0f * epsilon) {
                    Cap lower;
                    if (edgeLowerBound(state, strip, lower)) lowerBounds.replace(lower);
                    else lowerBounds.pop();
                }
                renderVertical(state, i + 1, upperBounds, lowerBounds, scanTop, top);
                return;
            }
        }

    // Case 3: Wall fully covers the active scan window
        if (strip.yTop < scanTop && strip.yBot > scanBot) {
            vstripAdd(state, strip, scanTop, scanBot);
            if (!(strip.flags & VSTRIP_FLAG_SEETHROUGH)) return;
        // Back ceiling / floor: the rays stay on that side of their plane
            if (w.flags & (strip.back ? WALL_FLAG_CEILING_FRONT : WALL_FLAG_CEILING_BACK)) {
                if (wTop < upperBound) {
                    Cap upper = capCeilingHeight(state, strip, !strip.back);
                    upper.h0 += epsilon;
                    upperBounds.push(upper);
                }
            }
            if (w.flags & (strip.back ? WALL_FLAG_FLOOR_FRONT : WALL_FLAG_FLOOR_BACK)) {
                if (wBot > lowerBound) {
                    Cap lower = capFloorHeight(state, strip, !strip.back);
                    lower.h0 -= epsilon;
                    lowerBounds.push(lower);
                }
            }
            continue;
        }

    // Case 4.a: Render area above the wall
        if (strip.yTop > scanTop) {
            int bot = std::min((int)strip.yTop, scanBot);
        // A ceiling on the lower bound is passed over: handle it as no ceiling
            bool hasCeiling = (strip.flags & VSTRIP_FLAG_HASCEILING) && wTop - lowerBound > 2.0f * epsilon;
            if (hasCeiling) {
            // The ceiling hides everything above the edge, draw it from below only
            // (from above is case 2)
                if ((flags & Renderer::FLAG_SURFACES) &&
                    camY < capCeilingHeight(state, strip, strip.back).h0)
                    vplaneDraw(state, strip, WALL_SURFACE_CEILING, scanTop, bot);
            } else {
            // Wall top edge: bound the rays passing over it
                Cap lower;
                bool push = edgeLowerBound(state, strip, lower);
                if (push) push = lowerBounds.push(lower);
                renderVertical(state, i + 1, upperBounds, lowerBounds, scanTop, bot);
                if (push) lowerBounds.pop();
            }
        }

    // Case 4.b: Render area below the wall
        if (strip.yBot < scanBot) {
            int top = std::max((int)strip.yBot, scanTop);
        // A floor on the upper bound is passed under: handle it as no floor
            bool hasFloor = (strip.flags & VSTRIP_FLAG_HASFLOOR) && upperBound - wBot > 2.0f * epsilon;
            if (hasFloor) {
            // The floor hides everything below the edge, draw it from above only
            // (from below is case 1)
                if ((flags & Renderer::FLAG_SURFACES) &&
                    camY > capFloorHeight(state, strip, strip.back).h0)
                    vplaneDraw(state, strip, WALL_SURFACE_FLOOR, top, scanBot);
            } else {
            // Wall bottom edge: bound the rays passing under it
                Cap upper;
                bool push = edgeUpperBound(state, strip, upper);
                if (push) push = upperBounds.push(upper);
                renderVertical(state, i + 1, upperBounds, lowerBounds, top, scanBot);
                if (push) upperBounds.pop();
            }
        }

    // Case 4.c: Wall straddles the scan window — draw it as-is
        if (strip.yBot < scanTop) return;
        if (strip.yTop > scanBot) return;
        vstripAdd(state, strip, scanTop, scanBot);
        if (strip.flags & VSTRIP_FLAG_SEETHROUGH) {
            scanTop = std::max((int)strip.yTop, scanTop);
            scanBot = std::min((int)strip.yBot, scanBot);
        // Back ceiling / floor: the rays stay on that side of their plane
            if (w.flags & (strip.back ? WALL_FLAG_CEILING_FRONT : WALL_FLAG_CEILING_BACK)) {
                if (wTop < upperBound) {
                    Cap upper = capCeilingHeight(state, strip, !strip.back);
                    upper.h0 += epsilon;
                    upperBounds.push(upper);
                }
            }
            if (w.flags & (strip.back ? WALL_FLAG_FLOOR_FRONT : WALL_FLAG_FLOOR_BACK)) {
                if (wBot > lowerBound) {
                    Cap lower = capFloorHeight(state, strip, !strip.back);
                    lower.h0 -= epsilon;
                    lowerBounds.push(lower);
                }
            }
            renderVertical(state, i + 1, upperBounds, lowerBounds, scanTop, scanBot);
        }
        return;
    }
}

/*****************************************************************************/
inline __m128 Renderer::sampleGlowmap(float gx, float gz)
{
    int x0 = ((int) gx) & (glowmapReso - 1);
    int z0 = ((int) gz) & (glowmapReso - 1);
    int x1 = (x0 + 1) & (glowmapReso - 1);
    int z1 = (z0 + 1) & (glowmapReso - 1);

    __m128 fx = _mm_set1_ps(gx - x0);
    __m128 fz = _mm_set1_ps(gz - z0);

    __m128 A = glowmap[z0 * glowmapReso + x0];
    __m128 B = glowmap[z0 * glowmapReso + x1];
    __m128 AB = _mm_add_ps(A, _mm_mul_ps(_mm_sub_ps(B, A), fx));

    __m128 C = glowmap[z1 * glowmapReso + x0];
    __m128 D = glowmap[z1 * glowmapReso + x1];
    __m128 CD = _mm_add_ps(C, _mm_mul_ps(_mm_sub_ps(D, C), fx));

    return _mm_add_ps(AB, _mm_mul_ps(_mm_sub_ps(CD, AB), fz));
}

/*****************************************************************************/
Renderer::Cap Renderer::capFloorHeight(const Context & state, const Strip & strip, bool back)
{
    const Wall & w = walls[strip.wallId];
    const Segment & s = segments[strip.wallId];

// Surface slope along the ray
// Away from the wall is towards the camera on the near side, away from it on the far side
    float rise = (nodes[w.nodeID2].pos.y() - nodes[w.nodeID1].pos.y()) * s.invLen;
    float drop = (back ? w.floorBackDrop : w.floorFrontDrop) * 0x1p-3;
    if (back != strip.back) drop = -drop;
    float d = (state.casterRayX * s.deltaX + state.casterRayZ * s.deltaZ) * s.invLen;
    float n = fabsf(state.casterRayX * s.deltaZ - state.casterRayZ * s.deltaX) * s.invLen;

// The surface passes through the wall edge at the hit distance
    Cap l;
    l.slope = rise * d + drop * n;
    l.h0 = strip.hBot - strip.dist * l.slope;
    return l;
}

Renderer::Cap Renderer::capCeilingHeight(const Context & state, const Strip & strip, bool back)
{
    const Wall & w = walls[strip.wallId];
    const Segment & s = segments[strip.wallId];

// Surface slope along the ray
// Away from the wall is towards the camera on the near side, away from it on the far side
    float rise = (nodes[w.nodeID2].pos.y() - nodes[w.nodeID1].pos.y()) * s.invLen;
    float drop = (back ? w.ceilingBackDrop : w.ceilingFrontDrop) * 0x1p-3;
    if (back != strip.back) drop = -drop;
    float d = (state.casterRayX * s.deltaX + state.casterRayZ * s.deltaZ) * s.invLen;
    float n = fabsf(state.casterRayX * s.deltaZ - state.casterRayZ * s.deltaX) * s.invLen;

// The surface passes through the wall edge at the hit distance
    Cap l;
    l.slope = rise * d + drop * n;
    l.h0 = strip.hTop - strip.dist * l.slope;
    return l;
}

/*****************************************************************************/
bool Renderer::edgeLowerBound(const Context & state, const Strip & strip, Cap & bound)
{
    const Wall & w = walls[strip.wallId];
    bool backCap = w.flags & (strip.back ? WALL_FLAG_CEILING_FRONT : WALL_FLAG_CEILING_BACK);

// Rays rising past the edge: the back ceiling plane, else a constant
    if (strip.yTop <= state.casterHorizon) {
        float slope = 0.0f;
        if (backCap)
            //slope = std::max(capCeilingHeight(state, strip, !strip.back).slope, 0.0f);
            slope = capCeilingHeight(state, strip, !strip.back).slope;
    // Back ceiling: keep the far wall carrying it, constant: clip the walls level with the edge
        float margin = backCap ? -epsilon : epsilon;
        bound = {strip.hTop + margin - slope * strip.dist, slope};
        return true;
    }

// Rays sinking past the edge: only a back ceiling bounds them
    if (!backCap) return false;
    bound = capCeilingHeight(state, strip, !strip.back);
    bound.h0 -= epsilon;
    return true;
}

bool Renderer::edgeUpperBound(const Context & state, const Strip & strip, Cap & bound)
{
    const Wall & w = walls[strip.wallId];
    bool backCap = w.flags & (strip.back ? WALL_FLAG_FLOOR_FRONT : WALL_FLAG_FLOOR_BACK);

// Rays sinking past the edge: the back floor plane, else a constant
    if (strip.yBot >= state.casterHorizon) {
        float slope = 0.0f;
        if (backCap)
            //slope = std::min(capFloorHeight(state, strip, !strip.back).slope, 0.0f);
            slope = capFloorHeight(state, strip, !strip.back).slope;
    // Back floor: keep the far wall carrying it, constant: clip the walls level with the edge
        float margin = backCap ? epsilon : -epsilon;
        bound = {strip.hBot + margin - slope * strip.dist, slope};
        return true;
    }

// Rays rising past the edge: only a back floor bounds them
    if (!backCap) return false;
    bound = capFloorHeight(state, strip, !strip.back);
    bound.h0 += epsilon;
    return true;
}

/*****************************************************************************/
void Renderer::vplaneDraw(Context & state, const Strip & strip, uint16_t sID, int scanTop, int scanBot)
{
    if (scanTop >= frameResoY) return;
    if (scanBot < 0) return;
    if (scanTop < 0) scanTop = 0;
    if (scanBot > frameResoY) scanBot = frameResoY;

    const Wall & w = walls[strip.wallId];
    Cap cap = (sID == WALL_SURFACE_FLOOR) ? capFloorHeight(state, strip, strip.back)
                                          : capCeilingHeight(state, strip, strip.back);
    float slope = cap.slope;
    float heightAbove = state.casterPos.y() - cap.h0;
    bool below = heightAbove > 0.0f;

    int yStart = below ? scanTop : scanBot - 1;
    int yInc = below ? 1 : -1;
    int yEnd = below ? scanBot : scanTop - 1;

    const Texture & t = textures[w.textureID];
    const Surface & s = w.surfaces[sID];
    uint32_t * tex = &t.pixels[s.id * t.block];

// Compute texture coordinates
    float sizeScale = t.size * 0x1p-8f;
    float uScale = s.scaleX * sizeScale;
    float vScale = s.scaleY * sizeScale;

// Texel coordinates along the ray, at mipmap level 0: u = uOrigin0 + uRay0 * adjacent
    float uOrigin0 = (state.casterPos.x() + w.offset.x() + s.shiftX) * uScale;
    float vOrigin0 = (state.casterPos.z() + w.offset.z() + s.shiftY) * vScale;
    float uRay0 = state.casterRayX * uScale;
    float vRay0 = state.casterRayZ * vScale;

// Ray distance per row: adjacent = opposite / denom, denom being linear in y
// Rays go through the row bottom edge, matching the (int) truncation of the wall strips
    float opposite = heightAbove * state.casterPC;
    if (opposite == 0.0f) return;

    float slopePC = slope * state.casterPC;
    float invProj = 1.0f / state.casterProjectionY;
    float denomStart = (yStart + 1.0f - state.casterHorizon) * invProj + slopePC;
    float denomInc = yInc * invProj;

// Distances shrink along the rows: skip the leading ones beyond the far plane
    float side = opposite >= 0.0f ? 1.0f : -1.0f;
    float skipRows = ceilf((opposite / fovDistanceFar - denomStart) * side / (denomInc * side));
    int skip = (int)std::clamp(skipRows, 0.0f, (float)((yEnd - yStart) * yInc));
    int yFar = yStart;
    yStart += skip * yInc;
    denomStart += skip * denomInc;

// Compute light
    float glowOriginX = state.casterPos.x() * glowmapScale + glowmapReso * 0.5f;
    float glowOriginZ = state.casterPos.z() * glowmapScale + glowmapReso * 0.5f;
    float glowRayX = state.casterRayX * glowmapScale;
    float glowRayZ = state.casterRayZ * glowmapScale;

    bool noFog = s.flags & SURFACE_FLAG_NO_FOG;

    __m128 ray = unpackColorToVectorSSE4(below ? floorRay : ceilingRay);
    const SurfaceLighting sl = surfaceLighting(s.flags, sunAmbientArgb, ray, glowBleed, flags & Renderer::FLAG_SUN_BLEED);
    const __m128 ill = sl.env;
    const __m128 bleed = sl.bleed;
    const __m128 bleedEnv = sl.bleedEnv;
    bool noGlow = !sl.glow;
    __m128 fog = unpackColorToVectorSSE4(fogFarColor);

// Compute AO zone boundary in screen space.
// Inverting adjacent = opposite / denom:
// y_target = horizon - 1.0 + proj * (opposite / adjacentAoEnd - slopePC)
    float adjacentStart = opposite / denomStart;
    float adjacentAoEnd = adjacentStart - occlusionLength;
    float aoScale = occlusionLength > 0.0f ? occlusionDarken / occlusionLength : 0.0f;

    int yAoEnd;
    if (!(flags & Renderer::FLAG_AMBIENT_OCCLUSION) || (w.flags & WALL_FLAG_NO_SHADOW) || !sl.occlusion)
        yAoEnd = yStart;
    else if (adjacentAoEnd <= 0.0f) yAoEnd = yEnd;
    else {
        float yTarget = state.casterHorizon - 1.0f + state.casterProjectionY * (opposite / adjacentAoEnd - slopePC);
        int yAoEndRaw = (int)yTarget;
        yAoEnd = below ? std::clamp(yAoEndRaw, yStart, yEnd)
                       : std::clamp(yAoEndRaw, yEnd, yStart);
    }

// Mipmap sections: the texels per column grow linearly with the distance, rate = adjacent * mmRate
// Level L (rate < 1: 0, rate < 2: 1, rate < 4: 2...) ends where adjacent drops under 2^(L-1) / mmRate
    int mmMax = (flags & Renderer::FLAG_MIPMAPPING) ? t.mmLevels - 1 : 0;
    float mmRate = std::max(fabsf(uScale), fabsf(vScale)) / (state.casterPC * state.casterProjectionX);

    auto levelAt = [&](float adjacent) -> int {
        float rate = adjacent * mmRate;
        if (!(rate >= 1.0f)) return 0;
        return std::min(highestBit((uint32_t) std::min(rate, 0x1p30f)) + 1, mmMax);
    };

    int level = 0;
    int yLevelEnd = yEnd;
    uint16_t mask = t.mask, size = t.size;
    float uOrigin = uOrigin0, vOrigin = vOrigin0, uRay = uRay0, vRay = vRay0;    // at the current level
    auto setLevel = [&](int l, int yFrom) {
        level = l;
        float scale = 1.0f / (float) (1 << l);
        uOrigin = uOrigin0 * scale; vOrigin = vOrigin0 * scale;
        uRay = uRay0 * scale; vRay = vRay0 * scale;
        mask = t.mask >> l;
        size = t.size >> l;
        tex = &t.pixels[t.mmOffsets[l] + s.id * (t.block >> (2 * l))];
        if (!l) {yLevelEnd = yEnd; return;}
    // Same inversion as the AO boundary
        float adjacent = (float) (1 << (l - 1)) / mmRate;
        float yTarget = state.casterHorizon - 1.0f + state.casterProjectionY * (opposite / adjacent - slopePC);
        int yLevelEndRaw = (int) std::clamp(yTarget, -2.0f, frameResoY + 1.0f);
        yLevelEnd = below ? std::clamp(yLevelEndRaw, yFrom, yEnd) : std::clamp(yLevelEndRaw, yEnd, yFrom);
    };

    uint32_t * ptr32 = &frame[state.casterColumn + frameResoX * yFar];

    bool applyFog = (fogFlags & 1) && !noFog;
    auto renderZones = [&](auto noGlowTag, auto applyFogTag) {
    // Type aliases rather than constexpr locals: MSVC refuses to let nested lambdas use those (C3495)
        using NoGlow = decltype(noGlowTag);
        using ApplyFog = decltype(applyFogTag);

    // Lambda: glow contribution for one pixel
        auto sampleGlow = [&](float adjacent) -> __m128 {
            if constexpr (NoGlow::value) {
                return fullBlack;
            } else {
                float glowX = glowOriginX + glowRayX * adjacent;
                float glowZ = glowOriginZ + glowRayZ * adjacent;
                return sampleGlowmap(glowX, glowZ);
            }
        };

    // Lambda: write the lit pixel, blending fog in
        auto shade = [&](__m128 lit, float adjacent) {
            if constexpr (ApplyFog::value) {
                float ff = std::clamp((fogDistanceOffset - adjacent) * fogDistanceInv, 0.0f, 1.0f);
                *ptr32 = packVectorToColorSSE4(vectorLinearSSE4(lit, fog, 1.0f - ff * ff));
            } else {
                *ptr32 = packVectorToColorSSE4(lit);
            }
        };

    // Lambda: one plane pixel with AO darkening (adjacentStart - adjacent) / occlusionLength, no clamping needed
        auto drawAmbientOcclusion = [&](float adjacent) {
            int u = floorToInt(uOrigin + uRay * adjacent);
            int v = floorToInt(vOrigin + vRay * adjacent);
            uint32_t co = tex[(u & mask) * size + (v & mask)];
            __m128 v4 = unpackColorToVectorSSE4(co);
            __m128 gm = sampleGlow(adjacent);
            __m128 glow = _mm_add_ps(gm, ill);
            float aoFactor = (1.0f - occlusionDarken) + (adjacentStart - adjacent) * aoScale;
            v4 = _mm_mul_ps(v4, _mm_set1_ps(aoFactor));
            __m128 bl = _mm_add_ps(gm, bleedEnv);
            __m128 lit = _mm_add_ps(_mm_mul_ps(v4, glow), _mm_mul_ps(_mm_mul_ps(bl, bl), bleed));
            shade(lit, adjacent);
            ptr32 += frameResoX * yInc;
        };

    // Lambda: one plane pixel with no AO overhead (hot path)
        auto drawPlain = [&](float adjacent) {
            int u = floorToInt(uOrigin + uRay * adjacent);
            int v = floorToInt(vOrigin + vRay * adjacent);
            uint32_t co = tex[(u & mask) * size + (v & mask)];
            __m128 v4 = unpackColorToVectorSSE4(co);
            __m128 gm = sampleGlow(adjacent);
            __m128 glow = _mm_add_ps(gm, ill);
            __m128 bl = _mm_add_ps(gm, bleedEnv);
            __m128 lit = _mm_add_ps(_mm_mul_ps(v4, glow), _mm_mul_ps(_mm_mul_ps(bl, bl), bleed));
            shade(lit, adjacent);
            ptr32 += frameResoX * yInc;
        };

    // Skipped rows: at the far distance
        setLevel(levelAt(fovDistanceFar), yStart);
        for (int k = 0; k < skip; k++)
            drawPlain(fovDistanceFar);

    // Walk a zone, far to near, stepping the mipmap level down at each section end
        float denom = denomStart;
        int y = yStart;
        setLevel(levelAt(adjacentStart), y);
        auto walk = [&](int yZoneEnd, auto draw) {
            while (y != yZoneEnd) {
                int yStop = below ? std::min(yZoneEnd, yLevelEnd) : std::max(yZoneEnd, yLevelEnd);
                for (; y != yStop; y += yInc) {
                    draw(opposite / denom);
                    denom += denomInc;
                }
                if (y == yLevelEnd && level > 0) setLevel(level - 1, y);
            }
        };

    // Ambient occlusion zone at the wall-base edge, then the plain zone
        walk(yAoEnd, drawAmbientOcclusion);
        walk(yEnd, drawPlain);
    };

    if (noGlow) {
        if (applyFog) renderZones(std::true_type{}, std::true_type{});
        else renderZones(std::true_type{}, std::false_type{});
    } else {
        if (applyFog) renderZones(std::false_type{}, std::true_type{});
        else renderZones(std::false_type{}, std::false_type{});
    }
}

/*****************************************************************************/
void Renderer::vstripAdd(Context & state, const Strip & strip, int scanTop, int scanBot)
{
    Strip newStrip = strip;
    if (newStrip.yBot > scanBot) {
        float dv = newStrip.vBot - newStrip.vTop;
        float dy = newStrip.yBot - newStrip.yTop;
        float r = (scanBot - newStrip.yTop) / dy;

        newStrip.yBot = scanBot;
        newStrip.vBot = newStrip.vTop + dv * r;
    }

    if (newStrip.yTop < scanTop) {
        float dv = newStrip.vBot - newStrip.vTop;
        float dy = newStrip.yBot - newStrip.yTop;
        float r = (scanTop - newStrip.yBot) / dy;

        newStrip.yTop = scanTop;
        newStrip.vTop = newStrip.vBot + dv * r;
    }
    state.vStripsClipped[state.vStripsClippedCount++] = newStrip;
}

/*****************************************************************************/
void Renderer::drawVStrips(Context & state)
{
    for (int i = 0; i < state.vStripsClippedCount; i++) {
        Strip & strip = state.vStripsClipped[(state.vStripsClippedCount - 1) - i];
        if (strip.flags & (VSTRIP_FLAG_INVISIBLE | VSTRIP_FLAG_BACKCULLED)) continue;
        vstripDraw(state, strip);
    }
}

void Renderer::vstripDraw(Context & state, const Strip & strip)
{
// Clip the strip
    int32_t scanTop = (int) strip.yTop;
    int32_t scanBot = (int) strip.yBot;

    if (scanBot < 0) return;
    if (scanTop >= frameResoY) return;
    if (scanBot == scanTop) return;

    //if (scanTop < 0) return;  // should not happen
    if (scanTop < 0) scanTop = 0;
    //if (scanBot > frameResoY) return;  // should not happen
    if (scanBot > frameResoY) scanBot = frameResoY;

// Check for click
    if (clickRegistered && (clickX == state.casterColumn)) {
        if (clickY >= scanTop && clickY < scanBot) {
            clickWallID = strip.wallId;
            clickRegistered = false;
        }
    }

// Fetch texture and surface
    const Wall & w = walls[strip.wallId];
    const Surface & s = w.surfaces[strip.back ? WALL_SURFACE_BACK : WALL_SURFACE_FRONT];
    const Texture & t = textures[w.textureID];

// Compute texture coords
    float sizeScale = t.size * 0x1p-8f;
    float uScale = s.scaleX * sizeScale;
    float vScale = s.scaleY * sizeScale;
// V sampled at the row bottom edges, like the strip coverage (16.16 fixed point)
    float vPerRow = (strip.vBot - strip.vTop) / (strip.yBot - strip.yTop);
    float vStart = strip.vTop + (scanTop + 1.0f - strip.yTop) * vPerRow;
    int32_t vFixed = floorToInt((vStart + s.shiftY) * vScale * 0x1p16f);
    int32_t vStep = (int32_t) (vPerRow * vScale * 0x1p16f);

    bool noFog = s.flags & SURFACE_FLAG_NO_FOG;

// Compute fog influence
    float fogFactor = 0.0f;
    __m128 fogColor = _mm_set1_ps(0);
    if (strip.flags & VSTRIP_FLAG_HIGHLIGHTED) {
        fogFactor = 0.25f;
        fogColor = unpackColorToVectorSSE4(0x00AAAAAA);

    }else if (fogFlags && !noFog) {
        float f = (fogDistanceOffset - strip.dist) * fogDistanceInv;
        float d = std::clamp(f, 0.0f, 1.0f);
        fogFactor = 1.0f - d * d;
        fogColor = unpackColorToVectorSSE4(fogFarColor);
    }

// Compute lights
    __m128 ray = unpackColorToVectorSSE4(strip.back ? w.rayBack : w.rayFront);
    const SurfaceLighting sl = surfaceLighting(s.flags, sunAmbientArgb, ray, glowBleed, flags & Renderer::FLAG_SUN_BLEED);
    const __m128 sun = sl.env;
    const __m128 bleed = sl.bleed;

    __m128 gm = fullBlack;
    if (sl.glow) {
        Segment & seg = segments[strip.wallId];
        float wallX = seg.beginX + strip.k * seg.deltaX;
        float wallZ = seg.beginZ + strip.k * seg.deltaZ;
        float glowX = wallX * glowmapScale + glowmapReso * 0.5f;
        float glowZ = wallZ * glowmapScale + glowmapReso * 0.5f;
        float glowSide = strip.back ? -GlowSampleDistance : GlowSampleDistance;
        glowX += seg.deltaZ * seg.invLen * glowSide;
        glowZ -= seg.deltaX * seg.invLen * glowSide;
        gm = sampleGlowmap(glowX, glowZ);
    }
    __m128 glow = _mm_add_ps(sun, gm);
    __m128 bl = _mm_add_ps(gm, sl.bleedEnv);
    __m128 glowBled = _mm_mul_ps(_mm_mul_ps(bl, bl), bleed);     // constant along the strip

// Mipmap level: the first one under a texel per row (vStep < 1: 0, vStep < 2: 1, vStep < 4: 2...)
// Masked surfaces stay on level 0, their key color must not blend
    int level = 0;
    uint32_t vStepAbs = (uint32_t) std::abs(vStep);
    if ((flags & Renderer::FLAG_MIPMAPPING) && !(s.flags & SURFACE_FLAG_ALPHA) && vStepAbs >= 0x10000)
        level = std::min(highestBit(vStepAbs) - 15, t.mmLevels - 1);
    vFixed >>= level;
    vStep >>= level;
    uint16_t mask = t.mask >> level;

// Configure the pointers
    int32_t u = floorToInt((strip.u + s.shiftX) * uScale) >> level;
    uint32_t * tex = &t.pixels[t.mmOffsets[level] + s.id * (t.block >> (2 * level))];
    tex = &tex[(u & mask) * (t.size >> level)];

    uint32_t * ptr32 = &frame[state.casterColumn + frameResoX * scanTop];

// Render a wall
    if (s.flags & SURFACE_FLAG_ALPHA) {
    // Lambda: alpha-masked strip / no ambient occlusion, transparent pixels skipped
        auto drawAlpha = [&]() {
            uint32_t co = tex[(vFixed >> 16) & mask];
            if (co != 0xFFFF00FF) {
                __m128 v4  = unpackColorToVectorSSE4(co);
                __m128 lit = _mm_add_ps(_mm_mul_ps(v4, glow), glowBled);
                *ptr32 = packVectorToColorSSE4(vectorLinearSSE4(lit, fogColor, fogFactor));
            }
            ptr32 += frameResoX;
            vFixed += vStep;
        };
        for (int y = scanTop; y < scanBot; y++) drawAlpha();
    }else{
    // Lambda: ambient occlusion scaling applied after lighting
        auto drawAmbientOcclusion = [&](float aoFactor) {
            uint32_t co = tex[(vFixed >> 16) & mask];
            __m128 v4  = _mm_mul_ps(unpackColorToVectorSSE4(co), _mm_set1_ps(aoFactor));
            __m128 lit = _mm_add_ps(_mm_mul_ps(v4, glow), glowBled);
            *ptr32 = packVectorToColorSSE4(vectorLinearSSE4(lit, fogColor, fogFactor));
            ptr32 += frameResoX;
            vFixed += vStep;
        };

    // Lambda: normal path
        auto drawPlain = [&]() {
            uint32_t co = tex[(vFixed >> 16) & mask];
            __m128 v4  = unpackColorToVectorSSE4(co);
            __m128 lit = _mm_add_ps(_mm_mul_ps(v4, glow), glowBled);
            *ptr32 = packVectorToColorSSE4(vectorLinearSSE4(lit, fogColor, fogFactor));
            ptr32 += frameResoX;
            vFixed += vStep;
        };

    // Occlusion boundaries, clamped to rendered range
        bool occlusion = (flags & Renderer::FLAG_AMBIENT_OCCLUSION) && sl.occlusion;
        int topAoEnd = occlusion ? std::clamp((int)strip.yDarkenTop, scanTop, scanBot) : scanTop;
        int botAoStart = occlusion ? std::clamp((int)strip.yDarkenBot, scanTop, scanBot) : scanBot;
        topAoEnd = std::min(topAoEnd, botAoStart);

    // Top gradient: (1-occlusionDarken) at yNoclipTop -> 1.0 at yDarkenTop
        float topLen = strip.yDarkenTop - strip.yNoclipTop;
        float topStep = topLen > 0.0f ? occlusionDarken / topLen : 0.0f;
        float topStart = (1.0f - occlusionDarken) + (scanTop - strip.yNoclipTop) * topStep;

    // Bottom gradient: 1.0 at yDarkenBot -> (1-occlusionDarken) at yNoclipBot
        float botLen = strip.yNoclipBot - strip.yDarkenBot;
        float botStep = botLen > 0.0f ? -occlusionDarken / botLen : 0.0f;
        float botStart = 1.0f + (botAoStart - strip.yDarkenBot) * botStep;

    // Top ambient occlusion zone
        float aoFactor = topStart;
        for (int y = scanTop; y < topAoEnd; y++, aoFactor += topStep)
            drawAmbientOcclusion(aoFactor);

    // Middle zone
        for (int y = topAoEnd; y < botAoStart; y++)
            drawPlain();

    // Bottom ambient occlusion zone
        aoFactor = botStart;
        for (int y = botAoStart; y < scanBot; y++, aoFactor += botStep)
            drawAmbientOcclusion(aoFactor);
    }
}

/*****************************************************************************/
void Renderer::buildTables()
{
    if (!panTable) return;

// Horizontal pan table (lens corrected)
    const float refX = projectionX();
    const float midX = frameResoX * 0.5f - 0.5f;
    for (int x = 0; x < frameResoX; x++) {
        float pan = atanf((x - midX) / refX);
        panTable[x] = pan;
    }
}

float Renderer::projectionX() const
{
    return 0.5f * frameResoX / tanf(fovAngle * D2R * 0.5f);
}

float Renderer::projectionY() const
{
    return frameResoY / tanf(fovAngle * D2R * 0.5f); // absence of 0.5f is a choice
}
