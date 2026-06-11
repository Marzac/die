/**
    DIE ENGINE
    Depth Integration Engine / A modern ray-caster
    (c) Fred's Lab 2024-2026
    Frédéric Meslin / info@fredslab.net
    SPDX-License-Identifier: MIT
    If used commercially, contributions, donations are highly appreciated.

    3d engine / configuration
*/

#include "renderer.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <algorithm>

/*****************************************************************************/
static void skipLine(FILE * file)
{
    while (true) {
        int c = fgetc(file);
        if (c == EOF) return;
        if (c == '\n') return;
    }
}

// Note: returns a pointer to a static buffer, single-threaded use only
static const char * getKey(FILE * file)
{
    static char key[128+1];
    int keyIndex = 0;

    while (true) {
        int c = fgetc(file);
        if (c == EOF) return nullptr;
        if (c == '\n') return nullptr;
        if (c == ' ' || c == '\t') continue;
        if (c == '=') break;
        if (keyIndex == 128) return nullptr;
        key[keyIndex++] = (char) c;
    }

    while (true) {
        int c = fgetc(file);
        if (c == EOF) return nullptr;
        if (c == '\n') return nullptr;
        if (c == ' ' || c == '\t') continue;
        ungetc(c, file);
        break;
    }

    key[keyIndex] = 0;
    return key;
}

// Round up to a power of two (the glowmap sampling relies on bit masks)
static int toPowerOfTwo(int v)
{
    int p = 32;
    while (p < v && p < 2048) p <<= 1;
    return p;
}

/*****************************************************************************/
void Renderer::configInit()
{
    flags = RENDERER_FLAGS_DEFAULT;

    frameResoX = DefaultFrameResoX;
    frameResoY = DefaultFrameResoY;
    glowmapSize = DefaultGlowmapReso;
    glowmapArea = DefaultGlowmapArea;

    nodesAllocated = DefaultNodesMax;
    wallsAllocated = DefaultWallsMax;
    texturesAllocated = DefaultTexturesMax;
    lightsAllocated = DefaultLightsMax;

    fovAngle = 80.0f;
    fovDistanceNear = 1.0f;
    fovDistanceFar = 8192.0f;

    sunAmbient = 0xDDDDDD;
    sunRayColor = 0xFFFFDD;
    sunRayDirection = QVector3D(-0.5f, -0.5f, 0.15f).normalized();
    sunAmbientStrength = 1.0f;
    sunRayStrength = 1.0f;

    fogDistanceNear = 0.0f;
    fogDistanceFar = 128.0f;
    fogFarColor = 0x000000;
    fogFlags = 1;

    occlusionLength = 3.0f;
    occlusionDarken = 0.9f;

    motionBlurFactor = 45.0f;
    vignetteInnerRadius = 75.0f;
    vignetteOuterRadius = 125.0f;
    gammaKRed = 1.0f;
    gammaKGreen = 1.0f;
    gammaKBlue = 1.0f;
}

/*****************************************************************************/
bool Renderer::configLoad(const QString & filename)
{
    FILE * file = fopen(filename.toLocal8Bit().constData(), "rb");
    if (!file) return false;

    while (true) {
    // Skip comments and empty lines
        int c = fgetc(file);
        if (c == EOF) break;
        if (c == '#') { skipLine(file); continue; }
        if (c == '\n' || c == '\r') continue;
        ungetc(c, file);

    // Read key
        const char * key = getKey(file);
        if (!key) { skipLine(file); continue; }

    // Read value
        char valueStr[128];
        if (fscanf(file, "%127s", valueStr) != 1) {
            skipLine(file);
            continue;
        }

    // Parse according to key (values clamped to sane ranges)
        if (strcmp(key, "ScreenResoX") == 0) frameResoX = std::clamp(atoi(valueStr), 64, 8192);
        else if (strcmp(key, "ScreenResoY") == 0) frameResoY = std::clamp(atoi(valueStr), 64, 8192);
        else if (strcmp(key, "GlowmapReso") == 0) glowmapSize = toPowerOfTwo(atoi(valueStr));
        else if (strcmp(key, "GlowmapArea") == 0) glowmapArea = std::max(1.0f, (float) atof(valueStr));
        else if (strcmp(key, "NodesMax") == 0) nodesAllocated = std::clamp(atoi(valueStr), 16, 65536);
        else if (strcmp(key, "WallsMax") == 0) wallsAllocated = std::clamp(atoi(valueStr), 16, 65536);
        else if (strcmp(key, "TexturesMax") == 0) texturesAllocated = std::clamp(atoi(valueStr), 1, 256);
        else if (strcmp(key, "LightsMax") == 0) lightsAllocated = std::clamp(atoi(valueStr), 1, 4096);

        else if (strcmp(key, "FOVAngle") == 0) fovAngle = atof(valueStr);
        else if (strcmp(key, "FOVDistanceNear") == 0) fovDistanceNear = atof(valueStr);
        else if (strcmp(key, "FOVDistanceFar") == 0) fovDistanceFar = atof(valueStr);

        else if (strcmp(key, "OcclusionLength") == 0) occlusionLength = atof(valueStr);
        else if (strcmp(key, "OcclusionDarken") == 0) occlusionDarken = atof(valueStr);

        else if (strcmp(key, "MotionBlurFactor") == 0) motionBlurFactor = atof(valueStr);
        else if (strcmp(key, "VignetteInnerRadius") == 0) vignetteInnerRadius = atof(valueStr);
        else if (strcmp(key, "VignetteOuterRadius") == 0) vignetteOuterRadius = atof(valueStr);
        else if (strcmp(key, "GammaKRed") == 0) gammaKRed = atof(valueStr);
        else if (strcmp(key, "GammaKGreen") == 0) gammaKGreen = atof(valueStr);
        else if (strcmp(key, "GammaKBlue") == 0) gammaKBlue = atof(valueStr);

        else if (strcmp(key, "Walls") == 0) {
            if (atoi(valueStr)) flags |= RENDERER_FLAG_WALLS; else flags &= ~RENDERER_FLAG_WALLS;
        } else if (strcmp(key, "Surfaces") == 0) {
            if (atoi(valueStr)) flags |= RENDERER_FLAG_SURFACES; else flags &= ~RENDERER_FLAG_SURFACES;
        } else if (strcmp(key, "Lights") == 0) {
            if (atoi(valueStr)) flags |= RENDERER_FLAG_LIGHTS; else flags &= ~RENDERER_FLAG_LIGHTS;
        } else if (strcmp(key, "AmbientOcclusion") == 0) {
            if (atoi(valueStr)) flags |= RENDERER_FLAG_AMBIENT_OCCLUSION; else flags &= ~RENDERER_FLAG_AMBIENT_OCCLUSION;
        } else if (strcmp(key, "MotionBlur") == 0) {
            if (atoi(valueStr)) flags |= RENDERER_FLAG_MOTIONBLUR; else flags &= ~RENDERER_FLAG_MOTIONBLUR;
        } else if (strcmp(key, "Vignette") == 0) {
            if (atoi(valueStr)) flags |= RENDERER_FLAG_VIGNETTE; else flags &= ~RENDERER_FLAG_VIGNETTE;
        } else if (strcmp(key, "Gamma") == 0) {
            if (atoi(valueStr)) flags |= RENDERER_FLAG_GAMMA; else flags &= ~RENDERER_FLAG_GAMMA;
        } else if (strcmp(key, "Multithreading") == 0) {
            if (atoi(valueStr)) flags |= RENDERER_FLAG_MULTITHREADING; else flags &= ~RENDERER_FLAG_MULTITHREADING;
        }

        skipLine(file);
    }

    fclose(file);
    return true;
}

bool Renderer::configSave(const QString & filename)
{
    FILE * file = fopen(filename.toLocal8Bit().constData(), "wb");
    if (!file) return false;

// ==== Write header ====
    fprintf(file, "# == DIE ENGINE CONFIG == \n");
    fprintf(file, "# Fred's Lab 2025\n");
    fprintf(file, "# 28.10.25 - V0.3\n");
    fprintf(file, "\n");

// ==== Write general settings ====
    fprintf(file, "# ==== Generals ==== \n");
    fprintf(file, "ScreenResoX = %d\n", frameResoX);
    fprintf(file, "ScreenResoY = %d\n", frameResoY);
    fprintf(file, "GlowmapReso = %d\n", glowmapSize);
    fprintf(file, "GlowmapArea = %.1f\n", glowmapArea);
    fprintf(file, "NodesMax = %d\n", nodesAllocated);
    fprintf(file, "WallsMax = %d\n", wallsAllocated);
    fprintf(file, "TexturesMax = %d\n", texturesAllocated);
    fprintf(file, "LightsMax = %d\n", lightsAllocated);
    fprintf(file, "\n");

// ==== Write render settings ====
    fprintf(file, "# ==== Render settings ====\n");
    fprintf(file, "FOVAngle = %3.1f\n", fovAngle);
    fprintf(file, "FOVDistanceNear = %4.2f\n", fovDistanceNear);
    fprintf(file, "FOVDistanceFar = %4.2f\n", fovDistanceFar);
    fprintf(file, "\n");

// ==== Write ambient occlusion settings ====
    fprintf(file, "# ==== Ambient occlusion ====\n");
    fprintf(file, "OcclusionLength = %4.2f\n", occlusionLength);
    fprintf(file, "OcclusionDarken = %4.2f\n", occlusionDarken);
    fprintf(file, "\n");

// ==== Write post-fx settings ====
    fprintf(file, "# ==== Post-fx ====\n");
    fprintf(file, "MotionBlurFactor = %3.1f\n", motionBlurFactor);
    fprintf(file, "VignetteInnerRadius = %4.2f\n", vignetteInnerRadius);
    fprintf(file, "VignetteOuterRadius = %4.2f\n", vignetteOuterRadius);
    fprintf(file, "GammaKRed = %4.2f\n", gammaKRed);
    fprintf(file, "GammaKGreen = %4.2f\n", gammaKGreen);
    fprintf(file, "GammaKBlue = %4.2f\n", gammaKBlue);
    fprintf(file, "\n");

// ==== Write render flag bits ====
    fprintf(file, "# ==== Render flags ====\n");
    fprintf(file, "Walls = %d\n",            (flags & RENDERER_FLAG_WALLS) ? 1 : 0);
    fprintf(file, "Surfaces = %d\n",         (flags & RENDERER_FLAG_SURFACES) ? 1 : 0);
    fprintf(file, "Lights = %d\n",           (flags & RENDERER_FLAG_LIGHTS) ? 1 : 0);
    fprintf(file, "AmbientOcclusion = %d\n", (flags & RENDERER_FLAG_AMBIENT_OCCLUSION) ? 1 : 0);
    fprintf(file, "MotionBlur = %d\n",       (flags & RENDERER_FLAG_MOTIONBLUR) ? 1 : 0);
    fprintf(file, "Vignette = %d\n",         (flags & RENDERER_FLAG_VIGNETTE) ? 1 : 0);
    fprintf(file, "Gamma = %d\n",            (flags & RENDERER_FLAG_GAMMA) ? 1 : 0);
    fprintf(file, "Multithreading = %d\n",   (flags & RENDERER_FLAG_MULTITHREADING) ? 1 : 0);
    fprintf(file, "\n");

    fclose(file);
    return true;
}
