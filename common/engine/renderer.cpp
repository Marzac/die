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

/*****************************************************************************/
Renderer renderer;
WorkerPool renderWorkers;

constexpr float epsilon = 0x1p-6f;

static const __m128 fullWhite = unpackColorToVectorSSE4(0x00FFFFFF);
static const __m128 fullBlack = unpackColorToVectorSSE4(0x00000000);

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
    memset(glowmapStill, 0, size * size * sizeof(__m128));
    glowmapReso = size;
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
        glowmapDirtyLast = {{0, glowmapReso, 0, glowmapReso}};
        return;
    }

 //
    if (flags & Renderer::FLAG_GLOWMAP_REBUILD) {
        memset(glowmapStill, 0, gmSize);
        if (flags & Renderer::FLAG_MULTITHREADING)
            glowmapConcurrent(glowmapStill, true);
        else glowmapChunk(glowmapStill, true, 0, glowmapReso);
        flags &= ~Renderer::FLAG_GLOWMAP_REBUILD;
        dirtyBoxes.append({0, glowmapReso, 0, glowmapReso});
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
void Renderer::texturesMash()
{
    for (int i = 0; i < texturesCount; i++) {
        Texture & t = textures[i];
        t.block = t.size * t.size;
        t.mask = t.size - 1;
    }
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
    texturesMash();
    lightsMash();

// Render frame
    viewPoint = vp;
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
// Render the strips
    const float angle = fovAngle * D2R;
    const float proj = frameResoY / tanf(angle * 0.5f);

// Clamp the shift to half the frame height
    state.casterVShift = frameResoY * viewPoint.tilt / angle;
    state.casterVShift = std::clamp(state.casterVShift, -0.5f * frameResoY, 0.5f * frameResoY);
    state.casterHorizon = frameResoY * 0.5f + state.casterVShift;
    state.casterProjection = proj;
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

    // Compute wall strips
        state.vStripsCount = 0;
        float casterX = state.casterPos.x();
        float casterZ = state.casterPos.z();
        float invRayX = state.casterRayX != 0.0f ? 1.0f / state.casterRayX : 32768.0f;
        float invRayZ = state.casterRayZ != 0.0f ? 1.0f / state.casterRayZ : 32768.0f;

        for (int i = 0; i < wallsCount; i++) {
            Segment & s = segments[i];
            float z = 0.0f;
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
                z = ((s.beginX - casterX) + k * s.deltaX) * invRayX;

            }else{
                float ratio = state.casterRayX * invRayZ;
                float delta = s.deltaZ * ratio - s.deltaX;
                if (delta == 0.0f) continue;
                float kd = (s.beginX - casterX) - (s.beginZ - casterZ) * ratio;
                if (delta > 0.0f) {if (kd < 0.0f || kd > delta) continue;}
                else {if (kd > 0.0f || kd < delta) continue;}
                k = kd / delta;
                z = ((s.beginZ - casterZ) + k * s.deltaZ) * invRayZ;
            }

        // Clipping the intersection
            if (z < fovDistanceNear) continue;
            if (z > fovDistanceFar) continue;

        // Find wall height
            Wall & w = walls[i];
            float div = (state.casterPC * proj) / z;
            float bot1 = nodes[w.nodeID1].pos.y();
            float bot2 = nodes[w.nodeID2].pos.y();
            float bot = bot1 + (bot2 - bot1) * k;
            float top = bot + w.height;

        // Create a strip object
            float casterY = state.casterPos.y();
            Strip & strip = state.vStrips[state.vStripsCount];
            strip.k = k;
            strip.yTop = state.casterHorizon - (top - casterY) * div;
            strip.yBot = state.casterHorizon - (bot - casterY) * div;
            strip.yNoclipTop  = strip.yTop;
            strip.yNoclipBot  = strip.yBot;
            strip.yDarkenTop  = strip.yTop + occlusionLength * div;
            strip.yDarkenBot  = strip.yBot - occlusionLength * div;
            strip.hTop = top;
            strip.hBot = bot;
            strip.vTop = top - bot;
            strip.vBot = 0.0f;
            strip.u = k * s.length;
            strip.dist = z;
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
            state.vStripsZOrders[state.vStripsCount++] = z * z;
        }

    // Sort the strips per depth
        std::sort(state.vStripsMap, state.vStripsMap + state.vStripsCount,
            [&](uint16_t a, uint16_t b) {
            float d1 = state.vStripsZOrders[a];
            float d2 = state.vStripsZOrders[b];
            return d1 < d2;
        });

    // Render surfaces
        state.vStripsClippedCount = 0;
        findFloorCeilings(state);

    // Track the floor / ceiling under the camera (center column only)
        if (state.casterColumn == frameResoX / 2) {
            lastCeiling = state.ceiling.h0;
            lastFloor = state.floor.h0;
        }

        QList<Cap> stackBelow;
        stackBelow.append({HeightMax, 0.0f});
        stackBelow.append({state.ceiling.h0 + epsilon, state.ceiling.slope});
        QList<Cap> stackAbove;
        stackAbove.append({HeightMin, 0.0f});
        stackAbove.append({state.floor.h0 - epsilon, state.floor.slope});
        renderVertical(state, 0, stackBelow, stackAbove, 0, frameResoY);

    // Render walls
        if (flags & Renderer::FLAG_WALLS) drawVStrips(state);
    }
}

/*****************************************************************************/
void Renderer::findFloorCeilings(Context & state)
{
    float camY = state.casterPos.y();
    int strips = state.vStripsCount;
    float secondLastCeiling = HeightMax;
    float secondLastFloor = HeightMin;

// Walk from the furthest wall to the closest
    QList<Cap> caps;
    for (int s = strips - 1; s >= 0; s--) {
        Strip & strip = state.vStrips[state.vStripsMap[s]];
        float wBot = strip.hBot;
        float wTop = strip.hTop;

    // Mask out surfaces covered, compared at the wall distance
        for (int i = caps.size() - 1; i >= 0; i--) {
            float h = caps[i].h0 + strip.dist * caps[i].slope;
            if (h >= wBot - epsilon && h <= wTop + epsilon)
                caps.removeAt(i);
        }

    // Add the ceiling / floor surfaces
        if (strip.flags & VSTRIP_FLAG_HASCEILING)
            caps.append(capCeilingHeight(state, strip, strip.back));
        if (strip.flags & VSTRIP_FLAG_HASFLOOR)
            caps.append(capFloorHeight(state, strip, strip.back));

    // Closest height above = ceiling, below = floor
        continue;
    }

// Check the last levels, compared under the camera
    state.ceiling = {HeightMax, 0.0f};
    state.floor = {HeightMin, 0.0f};
    for (const Cap & l : caps) {
        if (l.h0 > camY && l.h0 < state.ceiling.h0) state.ceiling = l;
        if (l.h0 < camY && l.h0 > state.floor.h0) state.floor = l;
    }
    if (state.ceiling.h0 == HeightMax) state.ceiling = {secondLastCeiling, 0.0f};
    if (state.floor.h0 == HeightMin) state.floor = {secondLastFloor, 0.0f};
}
/*
if (s != 1) continue;
for (const Cap & l : caps) {
    if (l.h0 > camY && l.h0 < secondLastCeiling) secondLastCeiling = l.h0;
    if (l.h0 < camY && l.h0 > secondLastFloor) secondLastFloor = l.h0;
}
*/
/*****************************************************************************/
void Renderer::renderVertical(Context & state, int depth,
                            QList<Cap> stackBelow, QList<Cap> stackAbove,
                            int scanTop, int scanBot)
{
    float camY = state.casterPos.y();
    for (int i = depth; i < state.vStripsCount; i++) {
        Strip & strip = state.vStrips[state.vStripsMap[i]];
        Wall & w = walls[strip.wallId];

    // Window bounds, compared at the wall distance
        float wBot = strip.hBot;
        float wTop = strip.hTop;
        Cap capBelow = stackBelow.last();
        Cap capAbove = stackAbove.last();
        float mustBelow = capBelow.h0 + capBelow.slope * strip.dist;
        float mustAbove = capAbove.h0 + capAbove.slope * strip.dist;

    // Case 0: Clip walls
        if (wTop < mustAbove) continue;
        if (wBot > mustBelow) continue;

    // Case 1: Floor surface seen from below
        if ((strip.flags & VSTRIP_FLAG_HASFLOOR) &&
            camY < capFloorHeight(state, strip, strip.back).h0) {
            int bot = std::min((int)strip.yBot, scanBot);
            if (bot < scanTop) bot = scanTop;
            if (bot < 0) bot = 0;

            if (wBot < mustBelow && wBot > mustAbove) {
                if (flags & Renderer::FLAG_SURFACES)
                    surfaceDraw(state, strip, WALL_SURFACE_FLOOR, scanTop, bot);
            // Pop the bound only if this wall is where its plane ends
                if (stackBelow.length() > 1 && mustBelow - wBot <= 2.0f * epsilon) stackBelow.removeLast();
                renderVertical(state, i + 1, stackBelow, stackAbove, bot, scanBot);
                return;
            }
        }

    // Case 2: Ceiling surface seen from above
        if ((strip.flags & VSTRIP_FLAG_HASCEILING) &&
            camY > capCeilingHeight(state, strip, strip.back).h0) {
            int top = std::max((int)strip.yTop, scanTop);
            if (top > scanBot) top = scanBot;
            if (top > frameResoY) top = frameResoY;

            if (wTop > mustAbove && wTop < mustBelow) {
                if (flags & Renderer::FLAG_SURFACES)
                    surfaceDraw(state, strip, WALL_SURFACE_CEILING, top, scanBot);
            // Pop the bound only if this wall is where its plane ends
                if (stackAbove.length() > 1 && wTop - mustAbove <= 2.0f * epsilon) stackAbove.removeLast();
                renderVertical(state, i + 1, stackBelow, stackAbove, scanTop, top);
                return;
            }
        }

    // Case 3: Wall fully covers the active scan window
        if (strip.yTop < scanTop && strip.yBot > scanBot) {
            vstripAdd(state, strip, scanTop, scanBot);
            if (!(strip.flags & VSTRIP_FLAG_SEETHROUGH)) return;
        // Through the wall, the rays pass below the top edge and above the bottom edge
        // Back ceiling / floor: the rays stay on that side of their plane, keep its slope
            if (w.flags & (strip.back ? WALL_FLAG_CEILING_FRONT : WALL_FLAG_CEILING_BACK)) {
                if (wTop < mustBelow) {
                    Cap mb = capCeilingHeight(state, strip, !strip.back);
                    mb.h0 += epsilon;
                    stackBelow.append(mb);
                }
            }
            if (w.flags & (strip.back ? WALL_FLAG_FLOOR_FRONT : WALL_FLAG_FLOOR_BACK)) {
                if (wBot > mustAbove) {
                    Cap ma = capFloorHeight(state, strip, !strip.back);
                    ma.h0 -= epsilon;
                    stackAbove.append(ma);
                }
            }
            continue;
        }

    // Case 4.a: Render area above the wall
        if (strip.yTop > scanTop) {
            int bot = std::min((int)strip.yTop, scanBot);
            if ((wTop > mustAbove) && (strip.flags & VSTRIP_FLAG_HASCEILING)) {
                if (flags & Renderer::FLAG_SURFACES)
                    surfaceDraw(state, strip, WALL_SURFACE_CEILING, scanTop, bot);
            } else {
            // Wall top edge: the rays rise past it, a constant is enough
            // Back ceiling: a surface beyond the wall, keep its slope
                bool push = false;
                Cap ma = capAbove;
                if (strip.yTop <= state.casterHorizon) {
                    if (wTop + epsilon > mustAbove) {
                        ma = {wTop + epsilon, 0.0f};
                        push = true;
                    }
                } else if (w.flags & (strip.back ? WALL_FLAG_CEILING_FRONT : WALL_FLAG_CEILING_BACK)) {
                    if (wTop > mustAbove) {
                        ma = capCeilingHeight(state, strip, !strip.back);
                        ma.h0 -= epsilon;
                        push = true;
                    }
                }
                if (push) stackAbove.append(ma);
                renderVertical(state, i + 1, stackBelow, stackAbove, scanTop, bot);
                if (push) stackAbove.removeLast();
            }
        }

    // Case 4.b: Render area below the wall
        if (strip.yBot < scanBot) {
            int top = std::max((int)strip.yBot, scanTop);
            if ((wBot < mustBelow) && (strip.flags & VSTRIP_FLAG_HASFLOOR)) {
                if (flags & Renderer::FLAG_SURFACES)
                    surfaceDraw(state, strip, WALL_SURFACE_FLOOR, top, scanBot);
            } else {
            // Wall bottom edge: the rays sink past it, a constant is enough
            // Back floor: a surface beyond the wall, keep its slope
                bool push = false;
                Cap mb = capBelow;
                if (strip.yBot >= state.casterHorizon) {
                    if (wBot - epsilon < mustBelow) {
                        mb = {wBot - epsilon, 0.0f};
                        push = true;
                    }
                } else if (w.flags & (strip.back ? WALL_FLAG_FLOOR_FRONT : WALL_FLAG_FLOOR_BACK)) {
                    if (wBot < mustBelow) {
                        mb = capFloorHeight(state, strip, !strip.back);
                        mb.h0 += epsilon;
                        push = true;
                    }
                }
                if (push) stackBelow.append(mb);
                renderVertical(state, i + 1, stackBelow, stackAbove, top, scanBot);
                if (push) stackBelow.removeLast();
            }
        }

    // Case 4.c: Wall straddles the scan window — draw it as-is
        if (strip.yBot < scanTop) return;
        if (strip.yTop > scanBot) return;
        vstripAdd(state, strip, scanTop, scanBot);
        if (strip.flags & VSTRIP_FLAG_SEETHROUGH) {
            scanTop = std::max((int)strip.yTop, scanTop);
            scanBot = std::min((int)strip.yBot, scanBot);
        // Through the wall, the rays pass below the top edge and above the bottom edge
        // Back ceiling / floor: the rays stay on that side of their plane, keep its slope
            if (w.flags & (strip.back ? WALL_FLAG_CEILING_FRONT : WALL_FLAG_CEILING_BACK)) {
                if (wTop < mustBelow) {
                    Cap mb = capCeilingHeight(state, strip, !strip.back);
                    mb.h0 += epsilon;
                    stackBelow.append(mb);
                }
            }
            if (w.flags & (strip.back ? WALL_FLAG_FLOOR_FRONT : WALL_FLAG_FLOOR_BACK)) {
                if (wBot > mustAbove) {
                    Cap ma = capFloorHeight(state, strip, !strip.back);
                    ma.h0 -= epsilon;
                    stackAbove.append(ma);
                }
            }
            renderVertical(state, i + 1, stackBelow, stackAbove, scanTop, scanBot);
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
void Renderer::surfaceDraw(Context & state, const Strip & strip, uint16_t sID, int scanTop, int scanBot)
{
    if (scanTop >= frameResoY) return;
    if (scanBot < 0) return;
    if (scanTop < 0) scanTop = 0;
    if (scanBot > frameResoY) scanBot = frameResoY;

    const Wall & w = walls[strip.wallId];
    Cap cap = (sID == WALL_SURFACE_FLOOR) ? capFloorHeight(state, strip, strip.back)
                                          : capCeilingHeight(state, strip, strip.back);
    float slope = cap.slope;
    float dy = state.casterPos.y() - cap.h0;
    bool below = dy > 0.0f;

    int yStart = below ? scanTop : scanBot - 1;
    int yInc = below ? 1 : -1;
    int yEnd = below ? scanBot : scanTop - 1;

    const Texture & t = textures[w.textureID];
    const Surface & s = w.surfaces[sID];
    uint32_t * tex = &t.pixels[s.id * t.block];

// Compute texture coordinates
    float sx = s.scaleX * (t.size >> 6);
    float sy = s.scaleY * (t.size >> 6);

    float ox = (state.casterPos.x() + w.offset.x() + s.shiftX) * sx;
    float oz = (state.casterPos.z() + w.offset.z() + s.shiftY) * sy;
    float rx = state.casterRayX * sx;
    float rz = state.casterRayZ * sy;

// Ray distance per row: adjacent = opposite / denom, denom being linear in y
// Rays go through the row bottom edge, matching the (int) truncation of the wall strips
    float opposite = dy * state.casterPC;
    float slopePC = slope * state.casterPC;
    float invProj = 1.0f / state.casterProjection;
    float denomStart = (yStart + 1.0f - state.casterHorizon) * invProj + slopePC;
    float denomInc = yInc * invProj;

// Rows beyond the surface horizon are pushed to the far distance
    auto distance = [&](float denom) -> float {
        float adjacent = opposite / denom;
        return (adjacent > 0.0f && adjacent < 65536.0f) ? adjacent : fovDistanceFar;
    };

// Compute light
    float gox = state.casterPos.x() * glowmapScale + glowmapReso * 0.5f;
    float goz = state.casterPos.z() * glowmapScale + glowmapReso * 0.5f;
    float grx = state.casterRayX * glowmapScale;
    float grz = state.casterRayZ * glowmapScale;

    bool noLight = s.flags & SURFACE_FLAG_NO_LIGHT;
    bool noGlow = s.flags & SURFACE_FLAG_NO_GLOW;
    bool noFog = s.flags & SURFACE_FLAG_NO_FOG;

    __m128 ray = unpackColorToVectorSSE4(below ? floorRay : ceilingRay);
    __m128 ill = noLight ? fullWhite : _mm_add_ps(sunAmbientArgb, ray);
    __m128 fog = unpackColorToVectorSSE4(fogFarColor);

// Compute AO zone boundary in screen space.
// Inverting adjacent = opposite / denom:
// y_target = horizon - 1.0 + proj * (opposite / adjacentAoEnd - slopePC)
    float adjacentStart = distance(denomStart);
    float adjacentAoEnd = adjacentStart - occlusionLength;
    float aoScale = occlusionLength > 0.0f ? occlusionDarken / occlusionLength : 0.0f;

    int yAoEnd;
    if (!(flags & Renderer::FLAG_AMBIENT_OCCLUSION) || (w.flags & WALL_FLAG_NO_SHADOW))
        yAoEnd = yStart;
    else if (adjacentAoEnd <= 0.0f) yAoEnd = yEnd;
    else {
        float yTarget = state.casterHorizon - 1.0f + state.casterProjection * (opposite / adjacentAoEnd - slopePC);
        int yAoEndRaw = (int)yTarget;
        yAoEnd = below ? std::clamp(yAoEndRaw, yStart, yEnd)
                       : std::clamp(yAoEndRaw, yEnd, yStart);
    }

    uint32_t * ptr32 = &frame[state.casterColumn + frameResoX * yStart];

    bool applyFog = (fogFlags & 1) && !noFog;
    auto renderZones = [&](auto noGlowTag, auto applyFogTag) {
        constexpr bool NoGlow = noGlowTag.value;
        constexpr bool ApplyFog = applyFogTag.value;

    // Lambda: glow contribution for one pixel
        auto sampleGlow = [&](float adjacent) -> __m128 {
            if constexpr (NoGlow) {
                return fullBlack;
            } else {
                float gx = gox + grx * adjacent;
                float gz = goz + grz * adjacent;
                return sampleGlowmap(gx, gz);
            }
        };

    // Lambda: write the lit pixel, blending fog in
        auto shade = [&](__m128 lit, float adjacent) {
            if constexpr (ApplyFog) {
                float ff = std::clamp((fogDistanceOffset - adjacent) * fogDistanceInv, 0.0f, 1.0f);
                *ptr32 = packVectorToColorSSE4(vectorLinearSSE4(lit, fog, 1.0f - ff * ff));
            } else {
                *ptr32 = packVectorToColorSSE4(lit);
            }
        };

    // Lambda: one surface pixel with AO darkening (adjacentStart - adjacent) / occlusionLength, no clamping needed
        auto drawAmbientOcclusion = [&](float adjacent) {
            int u = ox + rx * adjacent;
            int v = oz + rz * adjacent;
            uint32_t co = tex[(u & t.mask) * t.size + (v & t.mask)];
            __m128 v4 = unpackColorToVectorSSE4(co);
            __m128 glow = _mm_add_ps(sampleGlow(adjacent), ill);
            float aoFactor = (1.0f - occlusionDarken) + (adjacentStart - adjacent) * aoScale;
            v4 = _mm_mul_ps(v4, _mm_set1_ps(aoFactor));
            __m128 lit = _mm_add_ps(_mm_mul_ps(v4, glow), _mm_mul_ps(glow, glowBleed));
            shade(lit, adjacent);
            ptr32 += frameResoX * yInc;
        };

    // Lambda: one surface pixel with no AO overhead (hot path)
        auto drawPlain = [&](float adjacent) {
            int u = ox + rx * adjacent;
            int v = oz + rz * adjacent;
            uint32_t co = tex[(u & t.mask) * t.size + (v & t.mask)];
            __m128 v4 = unpackColorToVectorSSE4(co);
            __m128 glow = _mm_add_ps(sampleGlow(adjacent), ill);
            __m128 lit = _mm_add_ps(_mm_mul_ps(v4, glow), _mm_mul_ps(glow, glowBleed));
            shade(lit, adjacent);
            ptr32 += frameResoX * yInc;
        };

    // Ambient occlusion zone: wall-base edge
        float denom = denomStart;
        for (int y = yStart; y != yAoEnd; y += yInc) {
            drawAmbientOcclusion(distance(denom));
            denom += denomInc;
        }

    // Plain zone
        for (int y = yAoEnd; y != yEnd; y += yInc) {
            drawPlain(distance(denom));
            denom += denomInc;
        }
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
    float sx = s.scaleX * (t.size >> 6);
    float sy = s.scaleY * (t.size >> 6);
    int32_t v1 = (int32_t) (((strip.vTop + s.shiftY) * sy) * 0x1p16f);
    int32_t v2 = (int32_t) (((strip.vBot + s.shiftY) * sy) * 0x1p16f);
    int32_t dv = (v2 - v1) / (((int32_t) strip.yBot) - scanTop);

    bool noLight = s.flags & SURFACE_FLAG_NO_LIGHT;
    bool noGlow = s.flags & SURFACE_FLAG_NO_GLOW;
    bool noFog = s.flags & SURFACE_FLAG_NO_FOG;

// Compute fog influence
    float fogFactor = 0.0f;
    __m128 fogColor = _mm_set1_ps(0);
    if (strip.flags & VSTRIP_FLAG_HIGHLIGHTED) {
        fogFactor = 0.6f;
        fogColor = unpackColorToVectorSSE4(0x00AAAAAA);

    }else if (fogFlags && !noFog) {
        float f = (fogDistanceOffset - strip.dist) * fogDistanceInv;
        float d = std::clamp(f, 0.0f, 1.0f);
        fogFactor = 1.0f - d * d;
        fogColor = unpackColorToVectorSSE4(fogFarColor);
    }

// Compute lights
    __m128 ray = unpackColorToVectorSSE4(strip.back ? w.rayBack : w.rayFront);
    __m128 sun = noLight ? fullWhite : _mm_add_ps(sunAmbientArgb, ray);

    __m128 gm = fullBlack;
    if (!noGlow) {
        Segment & seg = segments[strip.wallId];
        float bx = seg.beginX + strip.k * seg.deltaX;
        float bz = seg.beginZ + strip.k * seg.deltaZ;
        float gx = bx * glowmapScale + glowmapReso * 0.5f;
        float gz = bz * glowmapScale + glowmapReso * 0.5f;
        float gside = strip.back ? -GlowSampleDistance : GlowSampleDistance;
        gx += seg.deltaZ * seg.invLen * gside;
        gz -= seg.deltaX * seg.invLen * gside;
        gm = sampleGlowmap(gx, gz);
    }
    __m128 glow = _mm_add_ps(sun, gm);

// Configure the pointers
    int32_t u = (strip.u + s.shiftX) * sx;
    uint32_t * tex = &t.pixels[s.id * t.block];
    tex = &tex[(u & t.mask) * t.size];

    uint32_t * ptr32 = &frame[state.casterColumn + frameResoX * scanTop];

// Render a wall
    if (s.flags & SURFACE_FLAG_ALPHA) {
    // Lambda: alpha-masked strip / no ambient occlusion, transparent pixels skipped
        auto drawAlpha = [&]() {
            uint32_t co = tex[(v1 >> 16) & t.mask];
            if (co != 0xFFFF00FF) {
                __m128 v4  = unpackColorToVectorSSE4(co);
                __m128 lit = _mm_mul_ps(v4, glow);
                lit = _mm_add_ps(lit, _mm_mul_ps(glow, glowBleed));
                *ptr32 = packVectorToColorSSE4(vectorLinearSSE4(lit, fogColor, fogFactor));
            }
            ptr32 += frameResoX;
            v1 += dv;
        };
        for (int y = scanTop; y < scanBot; y++) drawAlpha();
    }else{
    // Lambda: ambient occlusion scaling applied after lighting
        auto drawAmbientOcclusion = [&](float aoFactor) {
            uint32_t co = tex[(v1 >> 16) & t.mask];
            __m128 v4  = _mm_mul_ps(unpackColorToVectorSSE4(co), _mm_set1_ps(aoFactor));
            __m128 lit = _mm_add_ps(_mm_mul_ps(v4, glow), _mm_mul_ps(glow, glowBleed));
            *ptr32 = packVectorToColorSSE4(vectorLinearSSE4(lit, fogColor, fogFactor));
            ptr32 += frameResoX;
            v1 += dv;
        };

    // Lambda: normal path
        auto drawPlain = [&]() {
            uint32_t co = tex[(v1 >> 16) & t.mask];
            __m128 v4  = unpackColorToVectorSSE4(co);
            __m128 lit = _mm_mul_ps(v4, glow);
            lit = _mm_add_ps(lit, _mm_mul_ps(glow, glowBleed));
            *ptr32 = packVectorToColorSSE4(vectorLinearSSE4(lit, fogColor, fogFactor));
            ptr32 += frameResoX;
            v1 += dv;
        };

    // Occlusion boundaries, clamped to rendered range
        int topAoEnd = (flags & Renderer::FLAG_AMBIENT_OCCLUSION) ? std::clamp((int)strip.yDarkenTop, scanTop, scanBot) : scanTop;
        int botAoStart = (flags & Renderer::FLAG_AMBIENT_OCCLUSION) ? std::clamp((int)strip.yDarkenBot, scanTop, scanBot) : scanBot;
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
    const float angle = fovAngle * D2R;
    const float refX = 0.5f * frameResoX / tanf(angle * 0.5f);
    const float midX = frameResoX * 0.5f - 0.5f;
    for (int x = 0; x < frameResoX; x++) {
        float pan = atanf((x - midX) / refX);
        panTable[x] = pan;
    }
}
