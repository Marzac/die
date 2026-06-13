/**
    DIE ENGINE
    Depth Integration Engine / A modern ray-caster
    (c) Fred's Lab 2024-2026
    Frédéric Meslin / info@fredslab.net
    SPDX-License-Identifier: MIT
    If used commercially, contributions, donations are highly appreciated.

    3d engine
*/

#ifndef RENDERER_H
#define RENDERER_H

#include "mapobjects.h"
#include "workerpool.h"

#include <QVector2D>
#include <QVector3D>
#include <QImage>
#include <QList>

#include <atomic>
#include <stdint.h>

#include <smmintrin.h>

/*****************************************************************************/
typedef enum : uint32_t {
    RENDERER_FLAG_WALLS                   = 0x0001,
    RENDERER_FLAG_SURFACES                = 0x0002,
    RENDERER_FLAG_LIGHTS                  = 0x0004,
    RENDERER_FLAG_AMBIENT_OCCLUSION       = 0x0008,
    RENDERER_FLAG_GLOWMAP_REBUILD         = 0x0010,
    RENDERER_FLAG_MOTIONBLUR              = 0x0100,
    RENDERER_FLAG_VIGNETTE                = 0x0200,
    RENDERER_FLAG_GAMMA                   = 0x0400,
    RENDERER_FLAG_MULTITHREADING          = 0x1000,
    RENDERER_FLAG_ALPHA_FEATURES          = 0x2000,
    RENDERER_FLAGS_DEFAULT                = RENDERER_FLAG_WALLS | RENDERER_FLAG_SURFACES | RENDERER_FLAG_LIGHTS | RENDERER_FLAG_AMBIENT_OCCLUSION | RENDERER_FLAG_GAMMA,
} RENDERER_FLAGS;

/*****************************************************************************/
/**
    \brief Camera position and orientation
*/
typedef struct {
    QVector3D pos;
    QVector3D offset;
    float pan;
    float tilt;
} Viewpoint;

/*****************************************************************************/
constexpr float D2R = 3.14159265f / 180.0f;
constexpr float R2D = 180.0f / 3.14159265f;

/*****************************************************************************/
class Renderer
{
public:
/// \brief Default engine settings
    static constexpr int DefaultFrameResoX = 960;
    static constexpr int DefaultFrameResoY = 536;
    static constexpr int DefaultGlowmapReso = 256;
    static constexpr float DefaultGlowmapArea = 1024.0f;

    static constexpr int DefaultNodesMax = 4096;
    static constexpr int DefaultWallsMax = 2048;
    static constexpr int DefaultTexturesMax = 64;
    static constexpr int DefaultLightsMax = 64;

/// \brief Distance in front of a wall where the glowmap is sampled
    static constexpr float GlowSampleDistance = 1.0f;

/// \brief Bleeding factor of glowmap illumination to simulate color burning
    static constexpr float GlowBleedFactor = 0.25f;

/// \brief Sharpness of the light falloff curve; edge brightness at the glow
///        radius is 1 / (1 + GlowFalloffSharpness) (49 -> ~2%, no visible ring)
    static constexpr float GlowFalloffSharpness = 49.0f;

/// \brief World height (floor / ceiling) limits
    static constexpr float HeightMin = -16384.0f;
    static constexpr float HeightMax =  16384.0f;

/// \brief Default field of view
    static constexpr float DefaultFOVAngle = 80.0f;
    static constexpr float DefaultFOVDistanceNear = 1.0f;
    static constexpr float DefaultFOVDistanceFar = 8192.0f;

/// \brief Default sun lighting
    static constexpr uint32_t DefaultSunAmbient = 0xDDDDDD;
    static constexpr uint32_t DefaultSunRayColor = 0xFFFFDD;
    static constexpr float DefaultSunRayDirX = -0.5f;
    static constexpr float DefaultSunRayDirY = -0.5f;
    static constexpr float DefaultSunRayDirZ =  0.15f;
    static constexpr float DefaultSunAmbientStrength = 1.0f;
    static constexpr float DefaultSunRayStrength = 1.0f;

/// \brief Default distance fog
    static constexpr float DefaultFogDistanceNear = 0.0f;
    static constexpr float DefaultFogDistanceFar = 128.0f;
    static constexpr uint32_t DefaultFogFarColor = 0x000000;
    static constexpr uint16_t DefaultFogFlags = 1;

/// \brief Default ambient occlusion
    static constexpr float DefaultOcclusionLength = 3.0f;
    static constexpr float DefaultOcclusionDarken = 0.9f;

/// \brief Default post-fx
    static constexpr float DefaultMotionBlurFactor = 45.0f;
    static constexpr float DefaultVignetteInnerRadius = 75.0f;
    static constexpr float DefaultVignetteOuterRadius = 125.0f;
    static constexpr float DefaultGammaKRed = 1.0f;
    static constexpr float DefaultGammaKGreen = 1.0f;
    static constexpr float DefaultGammaKBlue = 1.0f;

    /// \brief Sentinel returned by clickGetWallID when no wall was picked
    static constexpr uint16_t ClickNoWall = 0xFFFF;

    /// \brief Renderer-side node (compact variant)
    typedef struct {
        QVector3D pos;
        uint16_t flags;
    } Node;

    /// \brief Renderer-side wall (compact variant)
    typedef struct {
        uint16_t nodeID1;
        uint16_t nodeID2;
        float height;

        QVector3D offset;
        Surface surfaces[WALL_SURFACES_COUNT];
        uint16_t textureID;
        uint16_t flags;

        uint32_t rayFront;
        uint32_t rayBack;
    } Wall;

    /// \brief Renderer-side texture strip (column of square tiles)
    typedef struct {
        uint32_t * pixels;
        uint16_t size;
        uint16_t count;
        uint16_t mask;
        uint32_t block;
    } Texture;

    /// \brief Bounding-box, half-open ([x0,x1) x [z0,z1))
    typedef struct {
        int x0, x1;
        int z0, z1;
    } BoundingBox;

    /// \brief Renderer-side light (compact variant)
    typedef struct {
        uint16_t nodeID;
        uint32_t color;
        float strength;
        float falloff;
        float falloffInv2;
        float x, z;
        BoundingBox boundingBox;
        __m128 argb;
        bool still;
    } Light;

    Renderer();

    /// \brief Allocate the object pools, frames and tables, load the config
    void init();

    /// \brief Release all the renderer resources
    void terminate();

    /// \brief Render the scene to the internal frame (see getImage)
    void render(Viewpoint & vp);

    /// \brief Frame rendered by the last render() call
    QImage * getImage() const {return image;}

    /// \brief Replace the whole RENDERER_FLAGS bitmask
    void setFlags(RENDERER_FLAGS flags);

    /// \brief Set or clear a single renderer flag
    void checkFlag(RENDERER_FLAGS flag, bool checked);
    RENDERER_FLAGS getFlags() const {return (RENDERER_FLAGS) flags;}

    /// \brief Request a glowmap resize (applied at the start of the next render)
    void setGlowmapSize(int size);
    int getGlowmapSize() const {return glowmapSize;}

    /// \brief Set the world area covered by the glowmap, in world units
    void setGlowmapArea(float area);
    float getGlowmapArea() const {return glowmapArea;}

    /// \brief Request a frame resize (applied at the start of the next render)
    void setFramesReso(int width, int height);
    int getFrameResoX() const {return frameResoX;}
    int getFrameResoY() const {return frameResoY;}

    /// \brief Set the horizontal field of view, in degrees
    void setFOVAngle(float angle);
    void setFOVDistanceNear(float distance);
    void setFOVDistanceFar(float distance);

    /// \brief Register a frame position to pick a wall during the next render
    void clickRegister(int x, int y);

    /// \brief Wall picked by the last registered click (see clickRegister)
    uint16_t clickGetWallID() const {return clickWallID;}

// Shared render state (written by Map::pass and Env::pass)
    uint32_t flags;             ///< active RENDERER_FLAGS bitmask

// Field of view
    float fovAngle;             ///< horizontal FOV, in degrees
    float fovDistanceNear;      ///< near clip distance, in world units
    float fovDistanceFar;       ///< far clip distance, in world units

// Sun lighting (fed by Env::pass)
    uint32_t sunAmbient;        ///< packed RGB ambient color
    uint32_t sunRayColor;       ///< packed RGB direct ray color
    QVector3D sunRayDirection;  ///< normalised ray direction
    float sunAmbientStrength;   ///< ambient intensity (0.0 .. 1.0)
    float sunRayStrength;       ///< ray intensity (0.0 .. 1.0)

// Distance fog (fed by Env::pass)
    float fogDistanceNear;      ///< fog start distance, in world units
    float fogDistanceFar;       ///< fog full-density distance, in world units
    uint32_t fogFarColor;       ///< packed RGB fog color
    uint16_t fogFlags;          ///< combination of FOG_FLAGS

// Ambient occlusion
    float occlusionLength;      ///< corner darkening reach, in world units
    float occlusionDarken;      ///< darkening strength (0.0 .. 1.0)

// Post-fx settings
    float motionBlurFactor;     ///< previous frame blend, in percent (0 .. 100)
    float vignetteInnerRadius;  ///< darkening start, in percent of half the frame height
    float vignetteOuterRadius;  ///< full black, in percent of half the frame height
    float gammaKRed;            ///< red smoothstep tone gain (1.0 = full curve)
    float gammaKGreen;          ///< green smoothstep tone gain
    float gammaKBlue;           ///< blue smoothstep tone gain

// Object pools, repopulated by Map::pass before each render
    Node * nodes;
    int nodesCount;
    int nodesAllocated;

    Wall * walls;
    int wallsCount;
    int wallsAllocated;

    Texture * textures;
    int texturesCount;
    int texturesAllocated;

    Light * lights;
    int lightsCount;
    int lightsAllocated;

// Floor / ceiling heights under the camera, updated by the last render
    float lastFloor;
    float lastCeiling;

private:
    static constexpr int StripsMax = 512;

    /// \brief Strip flags, translated from WALL_FLAGS in renderChunk()
    typedef enum : uint16_t {
        VSTRIP_FLAG_FREE        = 0x0000,
        VSTRIP_FLAG_INVISIBLE   = 0x0001,
        VSTRIP_FLAG_ALPHA       = 0x0004,
        VSTRIP_FLAG_HASCEILING  = 0x0100,
        VSTRIP_FLAG_HASFLOOR    = 0x0200,
        VSTRIP_FLAG_SEETHROUGH  = 0x0400,
        VSTRIP_FLAG_BACKCULLED  = 0x0800,
        VSTRIP_FLAG_HIGHLIGHTED = 0x1000,
    } VSTRIP_FLAGS;

    typedef struct {
        float k;
        float yTop;
        float yBot;
        float yNoclipTop;
        float yNoclipBot;
        float yDarkenTop;
        float yDarkenBot;
        float vTop;
        float vBot;
        float u;
        float dist;
        uint16_t wallId;
        bool back;
        uint16_t flags;
    } Strip;

    typedef struct {
        QVector3D casterPos;
        int casterX;
        float casterRx;
        float casterRz;
        float casterPC;
        float casterHorizon;
        float casterVShift;

        Strip vStrips[StripsMax];
        uint16_t vStripsMap[StripsMax];
        float vStripsZOrders[StripsMax];
        int vStripsCount;

        Strip vStripsClipped[StripsMax];
        int vStripsClippedCount;

        float currentCeiling;
        float currentFloor;
    } Context;
    Context renderStates[WorkerPool::WorkersMax];

    int frameResoX;
    int frameResoY;
    int glowmapSize;
    std::atomic<int> pendingFrameWidth;
    std::atomic<int> pendingFrameHeight;
    std::atomic<int> pendingGlowmapSize;
    Viewpoint viewPoint;

    QImage * image;
    uint32_t * frame;
    uint32_t * frameLast;
    bool framesAllocated;

    typedef struct {
        float bx, bz;
        float ex, ez;
        float dx, dz;
        float length;
        float invLen;
    } Segment;
    Segment * segments;

    uint32_t floorRay;
    uint32_t ceilingRay;

    __m128 sunAmbientArgb;
    __m128 sunRayColorArgb;

    float fogDistanceInv;
    float fogDistanceOffset;

    int clickX;
    int clickY;
    bool clickRegistered;
    uint16_t clickWallID;

    QVector2D sceneCornerBegin, sceneCornerEnd;
    float glowmapArea;
    float glowmapScale;
    float glowmapInvScale;
    __m128 * glowmap;
    __m128 * glowmapStill;
    __m128 glowBleed;

    QList<BoundingBox> glowmapDirtyBoxes;
    QList<BoundingBox> glowmapDirtyLast;

    float * pitchTable;
    float * panTable;

    void allocateObjects(int noNodes, int noWalls, int noTextures, int noLights);
    void allocateFrames(int width, int height);
    void allocateGlowmap(int size);
    bool allocatePixelBuffer(uint32_t *& pixels, int width, int height);
    void deallocate();

    void renderChunk(Context & state, int x1, int x2);
    void renderConcurrent();

    void fogSunMash();
    void wallsMash();
    void lightsMash();
    void glowmapChunk(__m128 * gm, bool still, int z1, int z2);
    void glowmapConcurrent(__m128 * gm, bool still);
    void texturesMash();

    void findFloorCeilings(Context & state);
    void renderVertical(Context & state, int depth, QList<float> stackBelow, QList<float> stackAbove, int scanTop, int scanBot);
    void drawVStrips(Context & state);

    void vstripAdd(Context & state, const Strip & strip, int scanTop, int scanBot);
    void vstripDraw(Context & state, const Strip & strip);
    void surfaceDraw(Context & state, const Wall & w, uint16_t sID, int scanTop, int scanBot, float height);

    void buildTables();

    void sceneBoundsInit();
    void sceneBoundsRegister(float x, float z);
    void sceneGetCoords(const float x, const float z, float & xs, float & zs);
    void worldGetCoords(const float xs, const float zs, float & x, float & z);

    __m128 sampleGlowmap(float gx, float gy);

    void configInit();
    bool configLoad(const QString & filename);
    bool configSave(const QString & filename);
};

extern Renderer renderer;

#endif // RENDERER_H
