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
#include "primitives.h"

#include <QImage>
#include <QList>

#include <atomic>
#include <stdint.h>

#include <smmintrin.h>

/*****************************************************************************/
class Renderer
{
public:
// Limits
    static constexpr int StripsMax = 512;                   ///< walls crossed by one column ray
    static constexpr float HeightMin = -16384.0f;           ///< lowest floor / ceiling height
    static constexpr float HeightMax =  16384.0f;           ///< highest floor / ceiling height
    static constexpr float Epsilon = 0x1p-6f;               ///< height tolerance for the walls and caps

// Default frame settings
    static constexpr int DefaultFrameResoX = 960;
    static constexpr int DefaultFrameResoY = 536;

// Default field of view
    static constexpr float DefaultFOVAngle = 80.0f;
    static constexpr float DefaultFOVDistanceNear = 1.0f;
    static constexpr float DefaultFOVDistanceFar = 8192.0f;

// Default glowmap settings
    static constexpr int DefaultGlowmapReso = 256;
    static constexpr float DefaultGlowmapArea = 1024.0f;

// Glowmap constants
    static constexpr float GlowSampleDistance = 1.0f;       ///< wall glow sampled this far in front of the wall
    static constexpr float GlowBleedFactor = 0.2f;
    static constexpr float GlowFalloffSharpness = 49.0f;

// Default collection sizes
    static constexpr int DefaultNodesMax = 4096;
    static constexpr int DefaultWallsMax = 2048;
    static constexpr int DefaultTexturesMax = 64;
    static constexpr int DefaultLightsMax = 64;

// Default sun lighting
    static constexpr uint32_t DefaultSunAmbient = 0xDDDDDD;
    static constexpr uint32_t DefaultSunRayColor = 0xFFFFDD;
    static constexpr float DefaultSunRayDirX = -0.5f;
    static constexpr float DefaultSunRayDirY = -0.5f;
    static constexpr float DefaultSunRayDirZ =  0.15f;
    static constexpr float DefaultSunAmbientStrength = 1.0f;
    static constexpr float DefaultSunRayStrength = 1.0f;

// Default distance fog
    static constexpr float DefaultFogDistanceNear = 0.0f;
    static constexpr float DefaultFogDistanceFar = 128.0f;
    static constexpr uint32_t DefaultFogFarColor = 0x000000;
    static constexpr uint16_t DefaultFogFlags = 1;

// Default ambient occlusion
    static constexpr float DefaultOcclusionLength = 3.0f;
    static constexpr float DefaultOcclusionDarken = 0.9f;

// Default post-fx
    static constexpr float DefaultMotionBlurFactor = 45.0f;
    static constexpr float DefaultVignetteInnerRadius = 75.0f;
    static constexpr float DefaultVignetteOuterRadius = 125.0f;
    static constexpr float DefaultGammaKRed = 1.0f;
    static constexpr float DefaultGammaKGreen = 1.0f;
    static constexpr float DefaultGammaKBlue = 1.0f;

    /// \brief Returned by clickGetWallID when no wall was picked
    static constexpr uint16_t ClickNoWall = 0xFFFF;

    /// \brief Renderer enable / disable flags
    typedef enum : uint32_t {
        FLAG_WALLS              = 0x0001,
        FLAG_SURFACES           = 0x0002,   ///< floors and ceilings
        FLAG_LIGHTS             = 0x0004,
        FLAG_AMBIENT_OCCLUSION  = 0x0008,
        FLAG_GLOWMAP_REBUILD    = 0x0010,   ///< redraw the still lights into the glowmap
        FLAG_SUN_BLEED          = 0x0020,   ///< the sun light bleeds too, not only the glowmap
        FLAG_MOTIONBLUR         = 0x0100,
        FLAG_VIGNETTE           = 0x0200,
        FLAG_GAMMA              = 0x0400,
        FLAG_MULTITHREADING     = 0x1000,
        FLAG_MIPMAPPING         = 0x2000,   ///< sample the mipmap levels
        FLAGS_DEFAULT           = FLAG_WALLS | FLAG_SURFACES | FLAG_LIGHTS | FLAG_AMBIENT_OCCLUSION | FLAG_GAMMA
                                | FLAG_MULTITHREADING | FLAG_MIPMAPPING,
    } FLAGS;

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
        float floorFrontDrop;       ///< surface slope away from the wall, in 1/8 per unit
        float floorBackDrop;
        float ceilingFrontDrop;
        float ceilingBackDrop;

        QVector3D offset;           ///< floor / ceiling texture offset
        Surface surfaces[WALL_SURFACES_COUNT];
        uint16_t textureID;
        uint16_t flags;

        uint32_t rayFront;          ///< sun ray color on the front side
        uint32_t rayBack;           ///< sun ray color on the back side
    } Wall;

    /// \brief Bounding box, half-open ([x0, x1) x [z0, z1))
    typedef struct {
        int beginX, beginZ;
        int endX, endZ;
    } BoundingBox;

    /// \brief Renderer-side light (compact variant)
    typedef struct {
        uint16_t nodeID;
        uint32_t color;
        float strength;
        float falloff;
        float falloffInv2;          ///< 1 / falloff^2
        float x, z;                 ///< world position
        BoundingBox boundingBox;    ///< glowmap cells reached
        __m128 argb;                ///< color * strength^2
        bool still;                 ///< drawn once into the still glowmap
    } Light;

    /// \brief Floor or ceiling plane: height at the probed position and world gradient (h = h0 + gx * dx + gz * dz)
    typedef struct {
        float h0;
        float gx, gz;
    } Cap3D;

    /// \brief Floor and ceiling planes around a position
    typedef struct {
        Cap3D floor;                ///< closest plane below, h0 = HeightMin if none
        Cap3D ceiling;              ///< closest plane above, h0 = HeightMax if none
    } Levels;

    Renderer();

    /// \brief Allocate the object pools, frames and tables, load the config
    void init();
    /// \brief Release all the renderer resources
    void terminate();

    /// \brief Render the scene to the internal frame (see getImage)
    void render(Viewpoint & vp);
    /// \brief Frame rendered by the last render() call
    QImage * getImage() const {return image;}

    /// \brief Replace the whole FLAGS bitmask
    void setFlags(FLAGS flags);
    /// \brief Set or clear a single flag
    void checkFlag(FLAGS flag, bool checked);
    FLAGS getFlags() const {return (FLAGS) flags;}

    /// \brief Request a glowmap resize, applied by the next render()
    void setGlowmapReso(int reso);
    int getGlowmapReso() const {return glowmapReso;}

    /// \brief World area covered by the glowmap, in world units
    void setGlowmapArea(float area);
    float getGlowmapArea() const {return glowmapArea;}

    /// \brief Request a frame resize, applied by the next render()
    void setFramesReso(int resoX, int resoY);
    int getFrameResoX() const {return frameResoX;}
    int getFrameResoY() const {return frameResoY;}

    /// \brief Horizontal field of view, in degrees
    void setFOVAngle(float angle);
    /// \brief Near and far clip distances, in world units
    void setFOVDistanceNear(float distance);
    void setFOVDistanceFar(float distance);

    /// \brief Pick the wall at a frame position during the next render()
    void clickRegister(int x, int y);
    /// \brief Wall picked by the last registered click, ClickNoWall if none
    uint16_t clickGetWallID() const {return clickWallID;}

    /// \brief Floor and ceiling at a position, found by casting one ray (pan in radians)
    /// Uses the walls of the last render(). The map is expected to delimit any
    /// floor / ceiling change with a wall, so a single ray finds the planes around pos.
    Levels probeLevels(const QVector3D & pos, float pan);

// Render state, fed by Map::pass and Env::pass
    uint32_t flags;             ///< active FLAGS bitmask

// Field of view
    float fovAngle;             ///< horizontal, in degrees
    float fovDistanceNear;      ///< in world units
    float fovDistanceFar;       ///< in world units

// Sun lighting
    uint32_t sunAmbient;        ///< packed RGB
    uint32_t sunRayColor;       ///< packed RGB
    QVector3D sunRayDirection;  ///< normalised
    float sunAmbientStrength;   ///< (0.0 .. 1.0)
    float sunRayStrength;       ///< (0.0 .. 1.0)

// Distance fog
    float fogDistanceNear;      ///< fog start, in world units
    float fogDistanceFar;       ///< full fog, in world units
    uint32_t fogFarColor;       ///< packed RGB
    uint16_t fogFlags;          ///< FOG_FLAGS

// Ambient occlusion
    float occlusionLength;      ///< darkening reach from the corners, in world units
    float occlusionDarken;      ///< darkening strength (0.0 .. 1.0)

// Post-fx
    float motionBlurFactor;     ///< previous frame blend, in percent (0 .. 100)
    float vignetteInnerRadius;  ///< darkening start, in percent of half the frame height
    float vignetteOuterRadius;  ///< full black, in percent of half the frame height
    float gammaKRed;            ///< smoothstep tone gain (1.0 = full curve)
    float gammaKGreen;
    float gammaKBlue;

// Object pools
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

// Floor and ceiling at the camera, probed by the last render()
    Levels levels;

private:
    /// \brief Strip flags, translated from WALL_FLAGS in castStrips()
    typedef enum : uint16_t {
        VSTRIP_FLAG_FREE        = 0x0000,
        VSTRIP_FLAG_INVISIBLE   = 0x0001,
        VSTRIP_FLAG_HASCEILING  = 0x0100,   ///< ceiling on the camera side
        VSTRIP_FLAG_HASFLOOR    = 0x0200,   ///< floor on the camera side
        VSTRIP_FLAG_SEETHROUGH  = 0x0400,   ///< invisible, alpha masked or back-culled
        VSTRIP_FLAG_BACKCULLED  = 0x0800,
        VSTRIP_FLAG_HIGHLIGHTED = 0x1000,
        VSTRIP_FLAG_NOSHADOW    = 0x2000,   ///< casts no AO on its floor / ceiling
    } VSTRIP_FLAGS;

    /// \brief Wall crossed by a column ray
    typedef struct {
        float k;                    ///< hit position along the wall (0.0 .. 1.0)
        float yTop;                 ///< top row
        float yBot;                 ///< bottom row
        float yNoclipTop;           ///< top row before clipping
        float yNoclipBot;           ///< bottom row before clipping
        float yDarkenTop;           ///< end row of the top AO gradient
        float yDarkenBot;           ///< start row of the bottom AO gradient
        float hTop;                 ///< top height, in world units
        float hBot;                 ///< bottom height, in world units
        float vTop;                 ///< texture v at yTop, in world units
        float vBot;                 ///< texture v at yBot, in world units
        float u;                    ///< texture u, in world units along the wall
        float dist;                 ///< distance along the ray
        uint16_t wallId;
        bool back;                  ///< the camera sees the back side
        uint16_t flags;             ///< VSTRIP_FLAGS
    } Strip;

    /// \brief Floor / ceiling plane along the ray: h(d) = h0 + slope * d (h0 at the camera)
    typedef struct {
        float h0;
        float slope;
    } Cap2D;

    /// \brief Linked list of caps, top first: each link lives in the frame of the recursion that pushed it
    typedef struct _CapLink {
        Cap2D cap;
        const _CapLink * prev;
    } CapLink;

    /// \brief Rays still open in a column: rows [yTop, yBot] and the cap stacks they travel between
    typedef struct {
        float yTop, yBot;
        const CapLink * capCeiling;
        const CapLink * capFloor;
    } Aperture;

    /// \brief Per-surface lighting terms, from the SURFACE_FLAGS
    /// The pixel is lit as: lit = texel * (env + glowmap) + (bleedEnv + glowmap)^2 * bleed
    /// The square only burns the brightly lit spots
    typedef struct {
        __m128 env;                 ///< sun ambient + ray, unit gain when fullbright
        __m128 bleed;               ///< bleed factor, zero when disabled
        __m128 bleedEnv;            ///< env light part that bleeds, black unless FLAG_SUN_BLEED
        bool glow;                  ///< sample the glowmap
        bool occlusion;             ///< ambient occlusion allowed
    } SurfaceLighting;

    /// \brief Per-thread column state
    typedef struct {
        QVector3D casterPos;
        int casterColumn;
        float casterRayX;
        float casterRayZ;
        float casterPC;             ///< 1 / cos(column pan), lens correction
        float casterHorizon;        ///< horizon row
        float casterVShift;         ///< horizon shift from the tilt, in rows
        float casterProjectionX;    ///< see projectionX()
        float casterProjectionY;    ///< see projectionY()

        Strip vStrips[StripsMax];           ///< walls crossed by the ray
        uint16_t vStripsMap[StripsMax];     ///< vStrips indices, near to far
        float vStripsZOrders[StripsMax];    ///< sort keys, dist^2
        int vStripsCount;

        Strip vStripsClipped[StripsMax];    ///< strips to draw, clipped to their rows
        int vStripsClippedCount;
    } Context;

    /// \brief Wall geometry cached by wallsMash()
    typedef struct {
        float beginX, beginZ;
        float endX, endZ;
        float deltaX, deltaZ;
        float length;
        float invLen;
    } Segment;

// Contexts
    Context renderStates[WorkerPool::WorkersMax];   ///< one per worker
    Context probeState;                             ///< for probeLevels()

// Frames
    int frameResoX;
    int frameResoY;
    int glowmapReso;
    std::atomic<int> pendingFrameResoX;     ///< resize requested by setFramesReso()
    std::atomic<int> pendingFrameResoY;
    std::atomic<int> pendingGlowmapReso;    ///< resize requested by setGlowmapReso()
    Viewpoint viewPoint;

    QImage * image;             ///< owns the frame pixels
    uint32_t * frame;           ///< image pixels
    uint32_t * frameLast;       ///< previous frame, for the motion blur
    bool framesAllocated;

// Scene, prepared each render()
    Segment * segments;

    uint32_t floorRay;          ///< sun ray color on the floors
    uint32_t ceilingRay;        ///< sun ray color on the ceilings

    __m128 sunAmbientArgb;
    __m128 sunRayColorArgb;

    float fogDistanceInv;       ///< 1 / fog length
    float fogDistanceOffset;    ///< distance of full fog

// Wall picking
    int clickX;
    int clickY;
    bool clickRegistered;
    uint16_t clickWallID;

// Glowmap
    float glowmapArea;
    float glowmapScale;         ///< cells per world unit
    float glowmapInvScale;
    __m128 * glowmap;           ///< still and moving lights
    __m128 * glowmapStill;      ///< still lights only
    __m128 glowBleed;

    QList<BoundingBox> glowmapDirtyBoxes;   ///< cells to redraw this frame
    QList<BoundingBox> glowmapDirtyLast;    ///< cells reached by the moving lights last frame

    float * panTable;           ///< pan angle per column, lens corrected

// Allocation
    void allocateObjects(int noNodes, int noWalls, int noTextures, int noLights);
    void allocateFrames(int width, int height);
    bool allocatePixelBuffer(uint32_t *& pixels, int width, int height);
    /// \brief Fill panTable
    void buildTables();
    /// \brief Pixels per unit of tan(angle), across columns (X) and rows (Y)
    float projectionX() const;
    float projectionY() const;
    void allocateGlowmap(int size);
    void deallocate();

// Scene preparation
    /// \brief Fog constants
    void fogSunMash();
    /// \brief Segments and wall sun rays
    void wallsMash();
    /// \brief Floor / ceiling sun rays, lights and glowmap
    void lightsMash();
    /// \brief World to glowmap cell coordinates, and back
    void sceneGetCoords(const float x, const float z, float & xs, float & zs);
    void worldGetCoords(const float xs, const float zs, float & x, float & z);
    /// \brief Draw the still or moving lights into the glowmap, rows [z1, z2) per chunk
    void glowmapConcurrent(__m128 * gm, bool still);
    void glowmapChunk(__m128 * gm, bool still, int z1, int z2);

// Column rendering
    /// \brief Render the columns, [x1, x2) per chunk
    void renderConcurrent();
    void renderChunk(Context & state, int x1, int x2);
    /// \brief Find the walls crossed by the column ray, sorted near to far
    void castStrips(Context & state);
    /// \brief Clip the walls and surfaces of a column through the apertures, from strip depth on
    void renderVertical(Context & state, int depth, Aperture ap);
    /// \brief Floor / ceiling of a wall side along the ray
    Cap2D capFloorHeight(const Context & state, const Strip & strip, bool back);
    Cap2D capCeilingHeight(const Context & state, const Strip & strip, bool back);
    /// \brief Floor of the rays past an edge: pops the floor if it ends there, pushes link (if any) if it ends or is tighter
    static const CapLink * linkFloor(const CapLink * floor, float hFloor, bool ends, CapLink * link, float d);
    /// \brief Ceiling of the rays past an edge: pops the ceiling if it ends there, pushes link (if any) if it ends or is tighter
    static const CapLink * linkCeiling(const CapLink * ceiling, float hCeiling, bool ends, CapLink * link, float d);

// Drawing
    /// \brief Draw a floor / ceiling surface on rows [scanTop, scanBot)
    void vplaneDraw(Context & state, const Strip & strip, uint16_t sID, int scanTop, int scanBot);
    /// \brief Queue a wall strip clipped to rows [scanTop, scanBot)
    void vstripAdd(Context & state, const Strip & strip, int scanTop, int scanBot);
    /// \brief Draw the queued strips, far to near
    void drawVStrips(Context & state);
    void vstripDraw(Context & state, const Strip & strip);
    static SurfaceLighting surfaceLighting(uint16_t surfaceFlags, __m128 ambient, __m128 ray, __m128 bleed, bool sunBleed);
    __m128 sampleGlowmap(float glowX, float glowZ);
    /// \brief SSE flooring
    static int floorToInt(float x);
    /// \brief Index of the highest set bit (x > 0)
    static int highestBit(uint32_t x);

// Probing
    /// \brief Floor / ceiling plane of a wall side, in world space
    Cap3D capPlane(const Context & state, const Strip & strip, bool back, bool ceiling);

// Config (die.ini)
    void configInit();
    bool configLoad(const QString & filename);
    bool configSave(const QString & filename);
};

extern Renderer renderer;

#endif // RENDERER_H
