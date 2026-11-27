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
// Defaults, then die.ini
    configInit();
    configLoad("die.ini");

    allocateFrames(frameResoX, frameResoY);
    allocateGlowmap(glowmapReso);
    allocateObjects(nodesAllocated, wallsAllocated, texturesAllocated, lightsAllocated);

    clickX = clickY = 0;
    clickRegistered = false;
    clickWallID = ClickNoWall;

    viewPoint = {};

    floorRay = 0x00FFFFFF;
    ceilingRay = 0x00FFFFFF;

// Write die.ini back, with the missing keys
    configSave("die.ini");
}

/*****************************************************************************/
void Renderer::allocateObjects(int noNodes, int noWalls, int noTextures, int noLights)
{
// Empty pools first: a failed allocation leaves the remaining ones empty
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

/*****************************************************************************/
bool Renderer::allocatePixelBuffer(uint32_t *& pixels, int width, int height)
{
    size_t size = width * height * sizeof(uint32_t);
    if (pixels) free(pixels);
    pixels = (uint32_t *) malloc(size);
    if (pixels) memset(pixels, 0, size);
    return pixels != nullptr;
}

/*****************************************************************************/
void Renderer::buildTables()
{
    if (!panTable) return;

// Pan angle per column, lens corrected
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
// Should be 0.5f * frameResoY like projectionX, left out for the look
    return frameResoY / tanf(fovAngle * D2R * 0.5f);
}

/*****************************************************************************/
void Renderer::allocateGlowmap(int size)
{
    glowmapReso = 0;
    delete[] glowmap;
    delete[] glowmapStill;
    glowmap = nullptr;
    glowmapStill = nullptr;
    if (!(glowmap = new (std::nothrow) __m128[size * size])) return;
    if (!(glowmapStill = new (std::nothrow) __m128[size * size])) {
        delete[] glowmap;
        glowmap = nullptr;
        return;
    }
// Both maps start black: the dynamic one is only rewritten inside the dirty boxes
    memset(glowmap, 0, size * size * sizeof(__m128));
    memset(glowmapStill, 0, size * size * sizeof(__m128));
    glowmapReso = size;

// The still lights must be drawn again into the new maps
    flags |= Renderer::FLAG_GLOWMAP_REBUILD;
}

/*****************************************************************************/
void Renderer::terminate()
{
    configSave("die.ini");
    deallocate();
}

/*****************************************************************************/
void Renderer::deallocate()
{
    delete image; image = nullptr;

    if (frameLast) {free(frameLast); frameLast = nullptr;}

    delete[] panTable; panTable = nullptr;
    delete[] glowmap; glowmap = nullptr;
    delete[] glowmapStill; glowmapStill = nullptr;
    framesAllocated = false;

    delete[] nodes; nodes = nullptr;
    delete[] walls; walls = nullptr;
    delete[] segments; segments = nullptr;
    delete[] textures; textures = nullptr;
    delete[] lights; lights = nullptr;
    nodesCount = wallsCount = texturesCount = lightsCount = 0;
    nodesAllocated = wallsAllocated = texturesAllocated = lightsAllocated = 0;
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
    flags |= Renderer::FLAG_GLOWMAP_REBUILD;
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
void Renderer::clickRegister(int x, int y)
{
    clickX = x;
    clickY = y;
    clickRegistered = true;
    clickWallID = ClickNoWall;
}

/*****************************************************************************/
void Renderer::render(Viewpoint & vp)
{
// Resizes requested by the other threads
    int pw = pendingFrameResoX.exchange(0);
    int ph = pendingFrameResoY.exchange(0);
    if (pw && ph) allocateFrames(pw, ph);

    int pg = pendingGlowmapReso.exchange(0);
    if (pg) allocateGlowmap(pg);

// Nothing to render until everything is allocated
    if (!framesAllocated) return;
    if (!nodesAllocated) return;
    if (!wallsAllocated) return;
    if (!texturesAllocated) return;
    if (!glowmapReso) return;

// Clear the frame
    memset(frame, 0, frameResoX * frameResoY * sizeof(uint32_t));

// Scene data for this frame
    fogSunMash();
    wallsMash();
    lightsMash();

// Floor and ceiling at the camera, then the columns
    viewPoint = vp;
    levels = probeLevels(viewPoint.pos + viewPoint.offset, viewPoint.pan);
    if (flags & Renderer::FLAG_MULTITHREADING) renderConcurrent();
    else renderChunk(renderStates[0], 0, frameResoX);

// Post-fx
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
void Renderer::fogSunMash()
{
// clearness = (offset - dist) * inv, an empty range fogs everything
    if (fogDistanceFar > fogDistanceNear) {
        fogDistanceInv = 1.0f / (fogDistanceFar - fogDistanceNear);
        fogDistanceOffset = fogDistanceFar;

    }else{
        fogDistanceInv = 0.0f;
        fogDistanceOffset = 0.0f;
    }
}

/*****************************************************************************/
void Renderer::wallsMash()
{
    QVector2D dir(sunRayDirection.x(), sunRayDirection.z());

    for (int i = 0; i < wallsCount; i++) {
        Wall & w = walls[i];
        Segment & s = segments[i];

        s.beginX = nodes[w.nodeID1].pos.x();
        s.beginZ = nodes[w.nodeID1].pos.z();
        s.endX = nodes[w.nodeID2].pos.x();
        s.endZ = nodes[w.nodeID2].pos.z();

        s.deltaX = s.endX - s.beginX;
        s.deltaZ = s.endZ - s.beginZ;
        s.length = sqrtf(s.deltaX * s.deltaX + s.deltaZ * s.deltaZ);

    // Zero-length walls are dropped
        if (s.length == 0.0f) {w.flags = WALL_FLAG_FREE; continue;}
        s.invLen = 1.0f / s.length;

    // Sun ray color on each side
        QVector2D normal(s.deltaZ * s.invLen, -s.deltaX * s.invLen);
        float facing = QVector2D::dotProduct(normal, dir);
        uint8_t front = (uint8_t) (std::clamp(facing * sunRayStrength, 0.0f, 1.0f) * 255.0f);
        w.rayFront = colorsScaleSSE4(sunRayColor, (uint16_t)front << 8);
        uint8_t back = (uint8_t) (std::clamp(-facing * sunRayStrength, 0.0f, 1.0f) * 255.0f);
        w.rayBack = colorsScaleSSE4(sunRayColor, (uint16_t)back << 8);
    }
}

/*****************************************************************************/
void Renderer::lightsMash()
{
// Glowmap scale: fixed area centered at the origin
    glowmapScale = glowmapReso / glowmapArea;
    glowmapInvScale = 1.0f / glowmapScale;

// Sun rays on floors and ceilings
    uint8_t kFloor = (uint8_t)(std::clamp(sunRayDirection.y() * sunRayStrength, 0.0f, 1.0f) * 255.0f);
    floorRay = colorsScaleSSE4(sunRayColor, (uint16_t)kFloor << 8);
    uint8_t kCeil = (uint8_t)(std::clamp(-sunRayDirection.y() * sunRayStrength, 0.0f, 1.0f) * 255.0f);
    ceilingRay = colorsScaleSSE4(sunRayColor, (uint16_t)kCeil << 8);

// Lights: moving ones mark the glowmap cells they reach as dirty
    sunAmbientArgb = unpackColorToVectorScaledSSE4(sunAmbient, sunAmbientStrength);
    sunRayColorArgb = unpackColorToVectorScaledSSE4(sunRayColor, sunRayStrength);
    QList<BoundingBox> dirtyBoxes;
    for (int j = 0; j < lightsCount; j++) {
        Light & l = lights[j];
        l.argb = unpackColorToVectorScaledSSE4(l.color, l.strength * l.strength);
        l.x = nodes[l.nodeID].pos.x();
        l.z = nodes[l.nodeID].pos.z();
        l.falloffInv2 = 1.0f / (l.falloff * l.falloff);

    // Glowmap cells within reach
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

// Glowmaps, black with the lights off
    glowBleed = _mm_set1_ps(GlowBleedFactor);
    const size_t gmSize = glowmapReso * glowmapReso * sizeof(__m128);
    if (!(flags & Renderer::FLAG_LIGHTS)) {
        memset(glowmap, 0, gmSize);
        glowmapDirtyLast = {{0, 0, glowmapReso, glowmapReso}};
        return;
    }

// Still lights: redrawn on a rebuild request only, the whole map is then dirty
    if (flags & Renderer::FLAG_GLOWMAP_REBUILD) {
        memset(glowmapStill, 0, gmSize);
        if (flags & Renderer::FLAG_MULTITHREADING)
            glowmapConcurrent(glowmapStill, true);
        else glowmapChunk(glowmapStill, true, 0, glowmapReso);
        flags &= ~Renderer::FLAG_GLOWMAP_REBUILD;
        dirtyBoxes.append({0, 0, glowmapReso, glowmapReso});
    }

// Moving lights: redraw their cells of this frame and of the last one
    glowmapDirtyBoxes = glowmapDirtyLast + dirtyBoxes;
    glowmapDirtyLast = std::move(dirtyBoxes);

    if (flags & Renderer::FLAG_MULTITHREADING) glowmapConcurrent(glowmap, false);
    else glowmapChunk(glowmap, false, 0, glowmapReso);
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
void Renderer::glowmapChunk(__m128 * gm, bool still, int z1, int z2)
{
// Moving lights start from the still map, in the dirty boxes only
    if (!still) {
        for (const BoundingBox & r : glowmapDirtyBoxes) {
            int cellZMin = std::max(r.beginZ, z1);
            int cellZMax = std::min(r.endZ, z2);
            for (int cellZ = cellZMin; cellZ < cellZMax; cellZ++) {
                int row = cellZ * glowmapReso;
                memcpy(&gm[row + r.beginX], &glowmapStill[row + r.beginX], (r.endX - r.beginX) * sizeof(__m128));
            }
        }
    }

    for (int j = 0; j < lightsCount; j++) {
        const Light & l = lights[j];
        if (l.still != still) continue;

        int cellZMin = std::max(l.boundingBox.beginZ, z1);
        int cellZMax = std::min(l.boundingBox.endZ, z2);

        for (int cellZ = cellZMin; cellZ < cellZMax; cellZ++) {
            __m128 * row = &gm[cellZ * glowmapReso];
            for (int cellX = l.boundingBox.beginX; cellX < l.boundingBox.endX; cellX++) {
                float x, z;
                worldGetCoords(cellX, cellZ, x, z);
                float toLightX = l.x - x;
                float toLightZ = l.z - z;

            // Cell lit unless a wall stands between it and the light
                bool masked = false;
                for (int i = 0; i < wallsCount; i++) {
                    const Wall & w = walls[i];
                    if (w.flags & WALL_FLAG_NO_SHADOW) continue;

                    const Segment & s = segments[i];
                    float div = toLightX * s.deltaZ - toLightZ * s.deltaX;
                    if (fabs(div) < Epsilon) continue;
                    if ((w.flags & WALL_FLAG_BACKCULLED) && div < 0.0f) continue;

                // t along the cell-to-light line, u along the wall
                    float scale = 1.0f / div;
                    float t = ((s.beginX - x) * s.deltaZ - (s.beginZ - z) * s.deltaX) * scale;
                    if (t < 0.0f || t > 1.0f) continue;
                    float u = ((s.beginX - x) * toLightZ - (s.beginZ - z) * toLightX) * scale;
                    if (u < 0.0f || u > 1.0f) continue;
                    masked = true;
                    break;
                }

                if (masked) continue;

                float dist2 = toLightX * toLightX + toLightZ * toLightZ;
                float falloff = 1.0f / (1.0f + GlowFalloffSharpness * dist2 * l.falloffInv2);
                row[cellX] = _mm_add_ps(row[cellX], _mm_mul_ps(l.argb, _mm_set1_ps(falloff)));
            }
        }
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

// Horizon row, shifted by the tilt by at most half the frame height
    state.casterVShift = frameResoY * viewPoint.tilt / angle;
    state.casterVShift = std::clamp(state.casterVShift, -0.5f * frameResoY, 0.5f * frameResoY);
    state.casterHorizon = frameResoY * 0.5f + state.casterVShift;
    state.casterProjectionX = projectionX();
    state.casterProjectionY = projectionY();
    state.casterPos = viewPoint.pos + viewPoint.offset;

// Create the strips map
    for (int i = 0; i < StripsMax; i++)
        state.vStripsMap[i] = i;

    for (state.casterColumn = x1; state.casterColumn < x2; state.casterColumn++) {
        float pan = panTable[state.casterColumn];
        state.casterRayX = cosf(pan + viewPoint.pan);
        state.casterRayZ = sinf(pan + viewPoint.pan);
        state.casterPC = 1.0f / cosf(pan);
        castStrips(state);

    // Camera floor and ceiling along this ray
        Cap2D ceiling = {levels.ceiling.h0, levels.ceiling.gx * state.casterRayX + levels.ceiling.gz * state.casterRayZ};
        Cap2D floor = {levels.floor.h0, levels.floor.gx * state.casterRayX + levels.floor.gz * state.casterRayZ};

    // The rays start between the camera floor and ceiling, the height limits under them
        CapLink ceilingOrigin = {{HeightMax, 0.0f}, NULL};
        CapLink floorOrigin = {{HeightMin, 0.0f}, NULL};
        CapLink ceilingLevel = {ceiling, &ceilingOrigin};
        CapLink floorLevel = {floor, &floorOrigin};

        Aperture ap;
        ap.yTop = 0;
        ap.yBot = frameResoY;
        ap.capCeiling = &ceilingLevel;
        ap.capFloor = &floorLevel;
        state.vStripsClippedCount = 0;
        renderVertical(state, 0, ap);

    // Walls, drawn far to near over the surfaces
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

// 1 / ray, large for a ray along an axis
    float invRayX = state.casterRayX != 0.0f ? 1.0f / state.casterRayX : 32768.0f;
    float invRayZ = state.casterRayZ != 0.0f ? 1.0f / state.casterRayZ : 32768.0f;

    for (int i = 0; i < wallsCount; i++) {
        Segment & s = segments[i];
        float dist = 0.0f;
        float k = 0.0f;

    // Hit: k along the wall, dist along the ray, solved on the ray's major axis
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

    // Within the clip distances
        if (dist < fovDistanceNear) continue;
        if (dist > fovDistanceFar) continue;

    // Wall heights at the hit
        Wall & w = walls[i];
        float rowsPerUnit = (state.casterPC * state.casterProjectionY) / dist;
        float bot1 = nodes[w.nodeID1].pos.y();
        float bot2 = nodes[w.nodeID2].pos.y();
        float bot = bot1 + (bot2 - bot1) * k;
        float top = bot + w.height;

    // Rows, AO gradient ends, heights and texture coordinates
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

        if ((strip.flags & VSTRIP_FLAG_SEETHROUGH) || (w.flags & WALL_FLAG_NO_SHADOW))
            strip.flags |= VSTRIP_FLAG_NOSHADOW;

        state.vStripsMap[state.vStripsCount] = state.vStripsCount;
        state.vStripsZOrders[state.vStripsCount++] = dist * dist;
    }

// Sort near to far
    std::sort(state.vStripsMap, state.vStripsMap + state.vStripsCount,
        [&](uint16_t a, uint16_t b) {
        float d1 = state.vStripsZOrders[a];
        float d2 = state.vStripsZOrders[b];
        return d1 < d2;
    });
}

/*****************************************************************************/
void Renderer::renderVertical(Context & state, int depth, Aperture ap)
{
    if (ap.yTop >= ap.yBot) return;
    float casterY = state.casterPos.y();

// Move a cap 2 Epsilon past its plane when the camera is on its hidden side
    auto nudgeUp = [&](Cap2D & cap) {if (cap.h0 > casterY) cap.h0 += 2.0f * Epsilon;};
    auto nudgeDown = [&](Cap2D & cap) {if (cap.h0 < casterY) cap.h0 -= 2.0f * Epsilon;};
    auto onCap = [&](float h, float hCap) {return fabsf(h - hCap) <= 2.0f * Epsilon;};

    for (int i = depth; i < state.vStripsCount; i++) {
        Strip & strip = state.vStrips[state.vStripsMap[i]];
        Wall & w = walls[strip.wallId];
        float d = strip.dist;

    // Cull walls outside the caps
        float hCeiling = ap.capCeiling->cap.h0 + d * ap.capCeiling->cap.slope;
        float hFloor = ap.capFloor->cap.h0 + d * ap.capFloor->cap.slope;
        if (strip.hTop < hFloor - Epsilon) continue;
        if (strip.hBot > hCeiling + Epsilon) continue;

    // Caps of the rays over the top edge and under the bottom edge
        bool nearCeiling = strip.flags & VSTRIP_FLAG_HASCEILING;
        bool nearFloor = strip.flags & VSTRIP_FLAG_HASFLOOR;
        bool farCeiling = w.flags & (strip.back ? WALL_FLAG_CEILING_FRONT : WALL_FLAG_CEILING_BACK);
        bool farFloor = w.flags & (strip.back ? WALL_FLAG_FLOOR_FRONT : WALL_FLAG_FLOOR_BACK);
        Cap2D farCeilingCap = capCeilingHeight(state, strip, !strip.back);
        Cap2D farFloorCap = capFloorHeight(state, strip, !strip.back);

        CapLink floorLink = {farCeiling ? farCeilingCap : farFloorCap, NULL};
        nudgeUp(floorLink.cap);
        bool floorEnds = onCap(strip.hTop, hFloor) || (nearFloor && onCap(strip.hBot, hFloor));
        const CapLink * floorOver = linkFloor(ap.capFloor, hFloor, floorEnds, farCeiling || farFloor ? &floorLink : NULL, d);

        CapLink ceilingLink = {farFloor ? farFloorCap : farCeilingCap, NULL};
        nudgeDown(ceilingLink.cap);
        bool ceilingEnds = onCap(strip.hBot, hCeiling) || (nearCeiling && onCap(strip.hTop, hCeiling));
        const CapLink * ceilingUnder = linkCeiling(ap.capCeiling, hCeiling, ceilingEnds, farFloor || farCeiling ? &ceilingLink : NULL, d);

    // Near surfaces, a ceiling above the camera is seen from below, a floor below it from above
        bool ceilingAbove = nearCeiling && capCeilingHeight(state, strip, strip.back).h0 > casterY;
        bool floorBelow = nearFloor && capFloorHeight(state, strip, strip.back).h0 < casterY;

    // Ceiling seen from above, not lying on the cap: hides all below, only the rays over it go on
        if (nearCeiling && !ceilingAbove && hCeiling - strip.hTop > 2.0f * Epsilon) {
            Aperture newAp = ap;
            newAp.capFloor = floorOver;
            if (strip.yTop < ap.yBot) {
                int top = std::max(ap.yTop, strip.yTop);
                vplaneDraw(state, strip, WALL_SURFACE_CEILING, top, ap.yBot);
                newAp.yBot = top;
            }
            renderVertical(state, i + 1, newAp);
            return;
        }

    // Floor seen from below, not lying on the cap: hides all above, only the rays under it go on
        if (nearFloor && !floorBelow && strip.hBot - hFloor > 2.0f * Epsilon) {
            Aperture newAp = ap;
            newAp.capCeiling = ceilingUnder;
            if (strip.yBot >= ap.yTop) {
                int bot = std::min(ap.yBot, strip.yBot);
                vplaneDraw(state, strip, WALL_SURFACE_FLOOR, ap.yTop, bot);
                newAp.yTop = bot;
            }
            renderVertical(state, i + 1, newAp);
            return;
        }

    // Ceiling seen from below fills the rows above the wall, floor seen from above the rows below
        if (ceilingAbove && strip.yTop >= ap.yTop)
            vplaneDraw(state, strip, WALL_SURFACE_CEILING, ap.yTop, std::min(ap.yBot, strip.yTop));
        if (floorBelow && strip.yBot < ap.yBot)
            vplaneDraw(state, strip, WALL_SURFACE_FLOOR, std::max(ap.yTop, strip.yBot), ap.yBot);

    // No near ceiling: the rays go on over the wall, no near floor: under it
        if (!nearCeiling) {
            Aperture newAp = ap;
            newAp.yBot = std::min(ap.yBot, strip.yTop);
            newAp.capFloor = floorOver;
            renderVertical(state, i + 1, newAp);
        }
        if (!nearFloor) {
            Aperture newAp = ap;
            newAp.yTop = std::max(ap.yTop, strip.yBot);
            newAp.capCeiling = ceilingUnder;
            renderVertical(state, i + 1, newAp);
        }

    // Rows of the wall
        int top = std::max(ap.yTop, strip.yTop);
        if (top >= ap.yBot) return;
        int bot = std::min(ap.yBot, strip.yBot);
        if (bot < ap.yTop) return;

    // Wall lying on a cap: not drawn, the rays go on through its rows
        if (strip.hTop < hFloor || strip.hBot > hCeiling) {
            ap.yTop = top;
            ap.yBot = bot;
            if (ap.yTop >= ap.yBot) return;
            continue;
        }

        vstripAdd(state, strip, top, bot);
        if (!(strip.flags & VSTRIP_FLAG_SEETHROUGH)) return;

    // See-through: the rays go on through the wall
        CapLink throughCeiling = {farCeilingCap, NULL};
        CapLink throughFloor = {farFloorCap, NULL};
        nudgeDown(throughCeiling.cap);
        nudgeUp(throughFloor.cap);

        Aperture newAp;
        newAp.yTop = top;
        newAp.yBot = bot;
        newAp.capCeiling = linkCeiling(ap.capCeiling, hCeiling, onCap(strip.hTop, hCeiling), farCeiling ? &throughCeiling : NULL, d);
        newAp.capFloor = linkFloor(ap.capFloor, hFloor, onCap(strip.hBot, hFloor), farFloor ? &throughFloor : NULL, d);
        renderVertical(state, i + 1, newAp);
        return;
    }
}

/*****************************************************************************/
Renderer::Cap2D Renderer::capFloorHeight(const Context & state, const Strip & strip, bool back)
{
    const Wall & w = walls[strip.wallId];
    const Segment & s = segments[strip.wallId];

// Surface slope along the ray
// Away from the wall is towards the camera on the near side, away from it on the far side
    float rise = (nodes[w.nodeID2].pos.y() - nodes[w.nodeID1].pos.y()) * s.invLen;
    float drop = (back ? w.floorBackDrop : w.floorFrontDrop) * 0x1p-3;
    if (back != strip.back) drop = -drop;
    float along = (state.casterRayX * s.deltaX + state.casterRayZ * s.deltaZ) * s.invLen;
    float across = fabsf(state.casterRayX * s.deltaZ - state.casterRayZ * s.deltaX) * s.invLen;

// Through the bottom edge at the hit distance
    Cap2D cap;
    cap.slope = rise * along + drop * across;
    cap.h0 = strip.hBot - strip.dist * cap.slope;
    return cap;
}

Renderer::Cap2D Renderer::capCeilingHeight(const Context & state, const Strip & strip, bool back)
{
    const Wall & w = walls[strip.wallId];
    const Segment & s = segments[strip.wallId];

// Same slope as capFloorHeight, from the ceiling drops
    float rise = (nodes[w.nodeID2].pos.y() - nodes[w.nodeID1].pos.y()) * s.invLen;
    float drop = (back ? w.ceilingBackDrop : w.ceilingFrontDrop) * 0x1p-3;
    if (back != strip.back) drop = -drop;
    float along = (state.casterRayX * s.deltaX + state.casterRayZ * s.deltaZ) * s.invLen;
    float across = fabsf(state.casterRayX * s.deltaZ - state.casterRayZ * s.deltaX) * s.invLen;

// Through the top edge at the hit distance
    Cap2D cap;
    cap.slope = rise * along + drop * across;
    cap.h0 = strip.hTop - strip.dist * cap.slope;
    return cap;
}

/*****************************************************************************/
const Renderer::CapLink * Renderer::linkFloor(const CapLink * floor, float hFloor, bool ends, CapLink * link, float d)
{
    const CapLink * result = ends && floor->prev ? floor->prev : floor;
    if (link && (ends || link->cap.h0 + d * link->cap.slope > hFloor)) {
        link->prev = result;
        result = link;
    }
    return result;
}

const Renderer::CapLink * Renderer::linkCeiling(const CapLink * ceiling, float hCeiling, bool ends, CapLink * link, float d)
{
    const CapLink * result = ends && ceiling->prev ? ceiling->prev : ceiling;
    if (link && (ends || link->cap.h0 + d * link->cap.slope < hCeiling)) {
        link->prev = result;
        result = link;
    }
    return result;
}

/*****************************************************************************/
void Renderer::vplaneDraw(Context & state, const Strip & strip, uint16_t sID, int scanTop, int scanBot)
{
    if (!(flags & Renderer::FLAG_SURFACES)) return;
    if (scanTop >= scanBot) return;

    const Wall & w = walls[strip.wallId];
    Cap2D cap = (sID == WALL_SURFACE_FLOOR) ? capFloorHeight(state, strip, strip.back)
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

// Level 0 texel along the ray: u = uOrigin0 + uRay0 * adjacent
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
    const __m128 env = sl.env;
    const __m128 bleed = sl.bleed;
    const __m128 bleedEnv = sl.bleedEnv;
    bool noGlow = !sl.glow;
    __m128 fogColor = unpackColorToVectorSSE4(fogFarColor);

// AO at the wall base: enabled, accepted by the surface, cast by the wall
    bool occlusion = (flags & Renderer::FLAG_AMBIENT_OCCLUSION) && sl.occlusion && !(strip.flags & VSTRIP_FLAG_NOSHADOW);

// AO zone boundary in screen space, inverting adjacent = opposite / denom:
// y_target = horizon - 1.0 + proj * (opposite / adjacentAoEnd - slopePC)
    float adjacentStart = opposite / denomStart;
    float adjacentAoEnd = adjacentStart - occlusionLength;
    float aoScale = occlusionLength > 0.0f ? occlusionDarken / occlusionLength : 0.0f;

    int yAoEnd;
    if (!occlusion) yAoEnd = yStart;
    else if (adjacentAoEnd <= 0.0f) yAoEnd = yEnd;
    else {
        float yTarget = state.casterHorizon - 1.0f + state.casterProjectionY * (opposite / adjacentAoEnd - slopePC);
        int yAoEndRaw = (int)yTarget;
        yAoEnd = below ? std::clamp(yAoEndRaw, yStart, yEnd)
                       : std::clamp(yAoEndRaw, yEnd, yStart);
    }

// Mipmap level from the texels per column, adjacent * mmRate: < 1: 0, < 2: 1, < 4: 2...
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
    auto setLevel = [&](int newLevel, int yFrom) {
        level = newLevel;
        float scale = 1.0f / (float) (1 << newLevel);
        uOrigin = uOrigin0 * scale; vOrigin = vOrigin0 * scale;
        uRay = uRay0 * scale; vRay = vRay0 * scale;
        mask = t.mask >> newLevel;
        size = t.size >> newLevel;
        tex = &t.pixels[t.mmOffsets[newLevel] + s.id * (t.block >> (2 * newLevel))];
        if (!newLevel) {yLevelEnd = yEnd; return;}
    // Same inversion as the AO boundary
        float adjacent = (float) (1 << (newLevel - 1)) / mmRate;
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
                return _mm_setzero_ps();
            } else {
                float glowX = glowOriginX + glowRayX * adjacent;
                float glowZ = glowOriginZ + glowRayZ * adjacent;
                return sampleGlowmap(glowX, glowZ);
            }
        };

    // Lambda: write the lit pixel, blending fog in
        auto shade = [&](__m128 lit, float adjacent) {
            if constexpr (ApplyFog::value) {
                float clearness = std::clamp((fogDistanceOffset - adjacent) * fogDistanceInv, 0.0f, 1.0f);
                *ptr32 = packVectorToColorSSE4(vectorLinearSSE4(lit, fogColor, 1.0f - clearness * clearness));
            } else {
                *ptr32 = packVectorToColorSSE4(lit);
            }
        };

    // Lambda: one plane pixel with AO darkening (adjacentStart - adjacent) / occlusionLength, no clamping needed
        auto drawAmbientOcclusion = [&](float adjacent) {
            int u = floorToInt(uOrigin + uRay * adjacent);
            int v = floorToInt(vOrigin + vRay * adjacent);
            uint32_t texel = tex[(u & mask) * size + (v & mask)];
            __m128 color = unpackColorToVectorSSE4(texel);
            __m128 glowLocal = sampleGlow(adjacent);
            __m128 light = _mm_add_ps(glowLocal, env);
            float aoFactor = (1.0f - occlusionDarken) + (adjacentStart - adjacent) * aoScale;
            color = _mm_mul_ps(color, _mm_set1_ps(aoFactor));
            __m128 bleedLight = _mm_add_ps(glowLocal, bleedEnv);
            __m128 lit = _mm_add_ps(_mm_mul_ps(color, light), _mm_mul_ps(_mm_mul_ps(bleedLight, bleedLight), bleed));
            shade(lit, adjacent);
            ptr32 += frameResoX * yInc;
        };

    // Lambda: one plane pixel with no AO overhead (hot path)
        auto drawPlain = [&](float adjacent) {
            int u = floorToInt(uOrigin + uRay * adjacent);
            int v = floorToInt(vOrigin + vRay * adjacent);
            uint32_t texel = tex[(u & mask) * size + (v & mask)];
            __m128 color = unpackColorToVectorSSE4(texel);
            __m128 glowLocal = sampleGlow(adjacent);
            __m128 light = _mm_add_ps(glowLocal, env);
            __m128 bleedLight = _mm_add_ps(glowLocal, bleedEnv);
            __m128 lit = _mm_add_ps(_mm_mul_ps(color, light), _mm_mul_ps(_mm_mul_ps(bleedLight, bleedLight), bleed));
            shade(lit, adjacent);
            ptr32 += frameResoX * yInc;
        };

    // Skipped rows: at the far distance
        setLevel(levelAt(fovDistanceFar), yStart);
        for (int k = 0; k < skip; k++)
            drawPlain(fovDistanceFar);

    // Walk a zone far to near, one mipmap level down at each section end
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

    // AO zone at the wall base, then the plain zone
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
// Rows of the strip, already clipped to the frame by renderVertical
    int32_t scanTop = (int) strip.yTop;
    int32_t scanBot = (int) strip.yBot;
    if (scanBot == scanTop) return;

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

// V sampled at the row bottom edges
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
        float clearness = std::clamp((fogDistanceOffset - strip.dist) * fogDistanceInv, 0.0f, 1.0f);
        fogFactor = 1.0f - clearness * clearness;
        fogColor = unpackColorToVectorSSE4(fogFarColor);
    }

// Compute lights
    __m128 ray = unpackColorToVectorSSE4(strip.back ? w.rayBack : w.rayFront);
    const SurfaceLighting sl = surfaceLighting(s.flags, sunAmbientArgb, ray, glowBleed, flags & Renderer::FLAG_SUN_BLEED);
    const __m128 env = sl.env;
    const __m128 bleed = sl.bleed;

    __m128 glowLocal = _mm_setzero_ps();
    if (sl.glow) {
        Segment & seg = segments[strip.wallId];
        float wallX = seg.beginX + strip.k * seg.deltaX;
        float wallZ = seg.beginZ + strip.k * seg.deltaZ;
        float glowX = wallX * glowmapScale + glowmapReso * 0.5f;
        float glowZ = wallZ * glowmapScale + glowmapReso * 0.5f;
        float glowSide = strip.back ? -GlowSampleDistance : GlowSampleDistance;
        glowX += seg.deltaZ * seg.invLen * glowSide;
        glowZ -= seg.deltaX * seg.invLen * glowSide;
        glowLocal = sampleGlowmap(glowX, glowZ);
    }
    __m128 light = _mm_add_ps(env, glowLocal);
    __m128 bleedLight = _mm_add_ps(glowLocal, sl.bleedEnv);
    __m128 glowBled = _mm_mul_ps(_mm_mul_ps(bleedLight, bleedLight), bleed);     // constant along the strip

// Mipmap level from the texels per row, vStep: < 1: 0, < 2: 1, < 4: 2...
    int level = 0;
    uint32_t vStepAbs = (uint32_t) std::abs(vStep);
    if ((flags & Renderer::FLAG_MIPMAPPING) && vStepAbs >= 0x10000)
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
            uint32_t texel = tex[(vFixed >> 16) & mask];
            if (texel != TEXTURE_KEY_COLOR) {
                __m128 color  = unpackColorToVectorSSE4(texel);
                __m128 lit = _mm_add_ps(_mm_mul_ps(color, light), glowBled);
                *ptr32 = packVectorToColorSSE4(vectorLinearSSE4(lit, fogColor, fogFactor));
            }
            ptr32 += frameResoX;
            vFixed += vStep;
        };
        for (int y = scanTop; y < scanBot; y++) drawAlpha();
    }else{
    // Lambda: ambient occlusion scaling applied after lighting
        auto drawAmbientOcclusion = [&](float aoFactor) {
            uint32_t texel = tex[(vFixed >> 16) & mask];
            __m128 color  = _mm_mul_ps(unpackColorToVectorSSE4(texel), _mm_set1_ps(aoFactor));
            __m128 lit = _mm_add_ps(_mm_mul_ps(color, light), glowBled);
            *ptr32 = packVectorToColorSSE4(vectorLinearSSE4(lit, fogColor, fogFactor));
            ptr32 += frameResoX;
            vFixed += vStep;
        };

    // Lambda: normal path
        auto drawPlain = [&]() {
            uint32_t texel = tex[(vFixed >> 16) & mask];
            __m128 color  = unpackColorToVectorSSE4(texel);
            __m128 lit = _mm_add_ps(_mm_mul_ps(color, light), glowBled);
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
Renderer::SurfaceLighting Renderer::surfaceLighting(uint16_t surfaceFlags, __m128 ambient, __m128 ray, __m128 bleed, bool sunBleed)
{
    bool noAmbient = surfaceFlags & SURFACE_FLAG_NO_AMBIENT;
    bool noRay = surfaceFlags & SURFACE_FLAG_NO_RAY;
    bool noGlow = surfaceFlags & SURFACE_FLAG_NO_GLOW;
    __m128 black = _mm_setzero_ps();

// Fullbright: the texels as they are, exact 1.0 gain (alpha lane cleared)
    if (noAmbient && noRay && noGlow) return {_mm_setr_ps(1.0f, 1.0f, 1.0f, 0.0f), black, black, false, false};

    SurfaceLighting l;
    l.env = _mm_add_ps(noAmbient ? black : ambient, noRay ? black : ray);
    l.bleed = (surfaceFlags & SURFACE_FLAG_NO_BLEED) ? black : bleed;
    l.bleedEnv = sunBleed ? l.env : black;
    l.glow = !noGlow;
    l.occlusion = !(surfaceFlags & SURFACE_FLAG_NO_AO);
    return l;
}

/*****************************************************************************/
inline __m128 Renderer::sampleGlowmap(float glowX, float glowZ)
{
    int x0 = ((int) glowX) & (glowmapReso - 1);
    int z0 = ((int) glowZ) & (glowmapReso - 1);
    int x1 = (x0 + 1) & (glowmapReso - 1);
    int z1 = (z0 + 1) & (glowmapReso - 1);

    __m128 fx = _mm_set1_ps(glowX - x0);
    __m128 fz = _mm_set1_ps(glowZ - z0);

    __m128 A = glowmap[z0 * glowmapReso + x0];
    __m128 B = glowmap[z0 * glowmapReso + x1];
    __m128 AB = _mm_add_ps(A, _mm_mul_ps(_mm_sub_ps(B, A), fx));

    __m128 C = glowmap[z1 * glowmapReso + x0];
    __m128 D = glowmap[z1 * glowmapReso + x1];
    __m128 CD = _mm_add_ps(C, _mm_mul_ps(_mm_sub_ps(D, C), fx));

    return _mm_add_ps(AB, _mm_mul_ps(_mm_sub_ps(CD, AB), fz));
}

/*****************************************************************************/
inline int Renderer::floorToInt(float x)
{
    __m128 v = _mm_set_ss(x);
    return _mm_cvtss_si32(_mm_floor_ss(v, v));
}

inline int Renderer::highestBit(uint32_t x)
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
    static constexpr int PlanesMax = 2 * StripsMax;
    Cap3D found[PlanesMax];
    int foundCount = 0;
    for (int depth = state.vStripsCount - 1; depth >= 0; depth--) {
        const Strip & strip = state.vStrips[state.vStripsMap[depth]];
        for (int i = foundCount - 1; i >= 0; i--) {
            const Cap3D & l = found[i];
            float h = l.h0 + strip.dist * (l.gx * state.casterRayX + l.gz * state.casterRayZ);
            if (h >= strip.hBot - Epsilon && h <= strip.hTop + Epsilon)
                found[i] = found[--foundCount];
        }

        if (strip.flags & VSTRIP_FLAG_HASCEILING)
            found[foundCount++] = capPlane(state, strip, strip.back, true);

        if (strip.flags & VSTRIP_FLAG_HASFLOOR)
            found[foundCount++] = capPlane(state, strip, strip.back, false);
    }

// Closest plane above = ceiling, below = floor
    Levels result = {{HeightMin, 0.0f, 0.0f}, {HeightMax, 0.0f, 0.0f}};
    float y = pos.y();
    for (int i = 0; i < foundCount; i++) {
        const Cap3D & l = found[i];
        if (l.h0 > y && l.h0 < result.ceiling.h0) result.ceiling = l;
        if (l.h0 < y && l.h0 > result.floor.h0) result.floor = l;
    }
    return result;
}

/*****************************************************************************/
Renderer::Cap3D Renderer::capPlane(const Context & state, const Strip & strip, bool back, bool ceiling)
{
    const Wall & w = walls[strip.wallId];
    const Segment & s = segments[strip.wallId];
    Cap2D cap = ceiling ? capCeilingHeight(state, strip, back) : capFloorHeight(state, strip, back);

// Plane gradient: rise along the wall, drop away from it on the surface side
    float rise = (nodes[w.nodeID2].pos.y() - nodes[w.nodeID1].pos.y()) * s.invLen;
    float drop = ceiling ? (back ? w.ceilingBackDrop : w.ceilingFrontDrop)
                         : (back ? w.floorBackDrop : w.floorFrontDrop);
    drop *= 0x1p-3f;
    float dirX = s.deltaX * s.invLen;
    float dirZ = s.deltaZ * s.invLen;
    float nx = back ? -dirZ : dirZ;     // front normal is (dirZ, -dirX)
    float nz = back ? dirX : -dirX;
    return {cap.h0, rise * dirX - drop * nx, rise * dirZ - drop * nz};
}
