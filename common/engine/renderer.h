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
    static constexpr int DefaultFrameResoX = 960;           ///< in pixels
    static constexpr int DefaultFrameResoY = 536;           ///< in pixels

// Default field of view
    static constexpr float DefaultFOVAngle = 80.0f;         ///< horizontal, in degrees
    static constexpr float DefaultFOVDistanceNear = 1.0f;   ///< in world units
    static constexpr float DefaultFOVDistanceFar = 8192.0f; ///< in world units

// Default glowmap settings
    static constexpr int DefaultGlowmapReso = 256;          ///< cells per side, power of 2
    static constexpr float DefaultGlowmapArea = 1024.0f;    ///< world units per side

// Glowmap constants
    static constexpr float GlowSampleDistance = 1.0f;       ///< wall glow sampled this far in front of the wall
    static constexpr float GlowBleedFactor = 0.2f;          ///< burn of the brightly lit spots
    static constexpr float GlowFalloffSharpness = 49.0f;    ///< k in 1 / (1 + k * d^2 / falloff^2)

// Default collection sizes
    static constexpr int DefaultNodesMax = 4096;            ///< node pool size
    static constexpr int DefaultWallsMax = 2048;            ///< wall pool size
    static constexpr int DefaultTexturesMax = 64;           ///< texture pool size
    static constexpr int DefaultLightsMax = 64;             ///< light pool size

// Default sun lighting
    static constexpr uint32_t DefaultSunAmbient = 0xDDDDDD;     ///< packed RGB
    static constexpr uint32_t DefaultSunRayColor = 0xFFFFDD;    ///< packed RGB
    static constexpr float DefaultSunRayDirX = -0.5f;           ///< ray direction, X
    static constexpr float DefaultSunRayDirY = -0.5f;           ///< same, Y
    static constexpr float DefaultSunRayDirZ =  0.15f;          ///< same, Z
    static constexpr float DefaultSunAmbientStrength = 1.0f;    ///< (0.0 .. 1.0)
    static constexpr float DefaultSunRayStrength = 1.0f;        ///< (0.0 .. 1.0)

// Default distance fog
    static constexpr float DefaultFogDistanceNear = 0.0f;       ///< fog start, in world units
    static constexpr float DefaultFogDistanceFar = 128.0f;      ///< full fog, in world units
    static constexpr uint32_t DefaultFogFarColor = 0x000000;    ///< packed RGB
    static constexpr uint16_t DefaultFogFlags = 1;              ///< FOG_FLAG_ENABLE

// Default ambient occlusion
    static constexpr float DefaultOcclusionLength = 3.0f;       ///< in world units
    static constexpr float DefaultOcclusionDarken = 0.9f;       ///< (0.0 .. 1.0)

// Default post-fx
    static constexpr float DefaultMotionBlurFactor = 45.0f;     ///< in percent
    static constexpr float DefaultVignetteInnerRadius = 75.0f;  ///< in percent of half the frame height
    static constexpr float DefaultVignetteOuterRadius = 125.0f; ///< in percent of half the frame height
    static constexpr float DefaultGammaKRed = 1.0f;             ///< smoothstep tone gain, red
    static constexpr float DefaultGammaKGreen = 1.0f;           ///< same, green
    static constexpr float DefaultGammaKBlue = 1.0f;            ///< same, blue

    /// \brief Returned by clickGetWallID when no wall was picked
    static constexpr uint16_t ClickNoWall = 0xFFFF;

    /// \brief Renderer enable / disable flags
    typedef enum : uint32_t {
        FLAG_WALLS              = 0x0001,   ///< draw the walls
        FLAG_SURFACES           = 0x0002,   ///< draw the floors and ceilings
        FLAG_LIGHTS             = 0x0004,   ///< glowmap lights
        FLAG_AMBIENT_OCCLUSION  = 0x0008,   ///< darken the corners
        FLAG_GLOWMAP_REBUILD    = 0x0010,   ///< redraw the still lights into the glowmap
        FLAG_SUN_BLEED          = 0x0020,   ///< the sun light bleeds too, not only the glowmap
        FLAG_MOTIONBLUR         = 0x0100,   ///< blend with the previous frame
        FLAG_VIGNETTE           = 0x0200,   ///< darken the frame borders
        FLAG_GAMMA              = 0x0400,   ///< smoothstep tone curve
        FLAG_MULTITHREADING     = 0x1000,   ///< render on the worker pool
        FLAG_MIPMAPPING         = 0x2000,   ///< sample the mipmap levels
        FLAGS_DEFAULT           = FLAG_WALLS | FLAG_SURFACES | FLAG_LIGHTS | FLAG_AMBIENT_OCCLUSION | FLAG_GAMMA
                                | FLAG_MULTITHREADING | FLAG_MIPMAPPING,    ///< flags at startup
    } FLAGS;

    /// \brief Renderer-side node (compact variant)
    typedef struct {
        QVector3D pos;              ///< world position
        uint16_t flags;             ///< NODE_FLAGS
    } Node;

    /// \brief Renderer-side wall (compact variant)
    typedef struct {
        uint16_t nodeID1;           ///< start node
        uint16_t nodeID2;           ///< end node
        float height;               ///< at the taller end, in world units above the nodes
        float slant;                ///< top rise from node 1 to node 2, in [-height, height]
        float floorFrontDrop;       ///< surface slope away from the wall, in 1/8 per unit
        float floorBackDrop;        ///< same, back side
        float ceilingFrontDrop;     ///< same, front ceiling
        float ceilingBackDrop;      ///< same, back ceiling

        QVector3D offset;           ///< floor / ceiling texture offset
        Surface surfaces[WALL_SURFACES_COUNT];  ///< indexed by WALL_SURFACES
        uint16_t textureID;         ///< texture of all the surfaces
        uint16_t flags;             ///< WALL_FLAGS

        uint32_t rayFront;          ///< sun ray color on the front side
        uint32_t rayBack;           ///< sun ray color on the back side
    } Wall;

    /// \brief Bounding box, half-open ([x0, x1) x [z0, z1))
    typedef struct {
        int beginX, beginZ;         ///< first cell
        int endX, endZ;             ///< past the last cell
    } BoundingBox;

    /// \brief Renderer-side light (compact variant)
    typedef struct {
        uint16_t nodeID;            ///< position node
        uint32_t color;             ///< packed RGB
        float strength;             ///< intensity, squared in argb
        float falloff;              ///< reach radius, in world units
        float falloffInv2;          ///< 1 / falloff^2
        float x, z;                 ///< world position
        BoundingBox boundingBox;    ///< glowmap cells reached
        __m128 argb;                ///< color * strength^2
        bool still;                 ///< drawn once into the still glowmap
    } Light;

    /// \brief Floor or ceiling plane: height at the probed position and world gradient (h = h0 + gx * dx + gz * dz)
    typedef struct {
        float h0;                   ///< height at the probed position
        float gx, gz;               ///< height change per world unit along X / Z
    } Cap3D;

    /// \brief Floor and ceiling planes around a position
    typedef struct {
        Cap3D floor;                ///< closest plane below, h0 = HeightMin if none
        Cap3D ceiling;              ///< closest plane above, h0 = HeightMax if none
    } Levels;

    /// \brief Empty pools and buffers, see init()
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
    /// \brief Active FLAGS bitmask
    FLAGS getFlags() const {return (FLAGS) flags;}

    /// \brief Request a glowmap resize, applied by the next render()
    void setGlowmapReso(int reso);
    /// \brief Glowmap cells per side
    int getGlowmapReso() const {return glowmapReso;}

    /// \brief World area covered by the glowmap, in world units
    void setGlowmapArea(float area);
    /// \brief World area covered by the glowmap
    float getGlowmapArea() const {return glowmapArea;}

    /// \brief Request a frame resize, applied by the next render()
    void setFramesReso(int resoX, int resoY);
    /// \brief Frame width, in pixels
    int getFrameResoX() const {return frameResoX;}
    /// \brief Frame height, in pixels
    int getFrameResoY() const {return frameResoY;}

    /// \brief Horizontal field of view, in degrees
    void setFOVAngle(float angle);
    /// \brief Near clip distance, in world units
    void setFOVDistanceNear(float distance);
    /// \brief Far clip distance, in world units
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
    float gammaKGreen;          ///< same, green
    float gammaKBlue;           ///< same, blue

// Object pools
    Node * nodes;               ///< node pool
    int nodesCount;             ///< used nodes
    int nodesAllocated;         ///< node pool size

    Wall * walls;               ///< wall pool
    int wallsCount;             ///< used walls
    int wallsAllocated;         ///< wall pool size

    Texture * textures;         ///< texture pool
    int texturesCount;          ///< used textures
    int texturesAllocated;      ///< texture pool size

    Light * lights;             ///< light pool
    int lightsCount;            ///< used lights
    int lightsAllocated;        ///< light pool size

// Probe
    Levels levels;              ///< floor and ceiling at the camera, probed by the last render()

private:
    /// \brief Strip flags, translated from WALL_FLAGS in castStrips()
    typedef enum : uint16_t {
        VSTRIP_FLAG_FREE        = 0x0000,   ///< no flag
        VSTRIP_FLAG_INVISIBLE   = 0x0001,   ///< not drawn
        VSTRIP_FLAG_HASCEILING  = 0x0100,   ///< ceiling on the camera side
        VSTRIP_FLAG_HASFLOOR    = 0x0200,   ///< floor on the camera side
        VSTRIP_FLAG_SEETHROUGH  = 0x0400,   ///< invisible, alpha masked or back-culled
        VSTRIP_FLAG_BACKCULLED  = 0x0800,   ///< back side seen, not drawn
        VSTRIP_FLAG_HIGHLIGHTED = 0x1000,   ///< selected in the editor
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
        float zOrder;               ///< sort key: dist^2, ties from the horizon outwards
        uint16_t wallId;            ///< index in walls and segments
        bool back;                  ///< the camera sees the back side
        uint16_t flags;             ///< VSTRIP_FLAGS
    } Strip;

    /// \brief Floor / ceiling plane along the ray: h(d) = h0 + slope * d (h0 at the camera)
    typedef struct {
        float h0;                   ///< height at the camera
        float slope;                ///< height change per unit along the ray
    } Cap2D;

    /// \brief Linked list of caps, top first: each link lives in the frame of the recursion that pushed it
    typedef struct _CapLink {
        Cap2D cap;                  ///< this link's cap
        const _CapLink * prev;      ///< link below, null at the root
    } CapLink;

    /// \brief Rays still open in a column: rows [yTop, yBot] and the cap stacks they travel between
    typedef struct {
        float yTop, yBot;           ///< open rows
        const CapLink * capCeiling; ///< ceiling stack, top link
        const CapLink * capFloor;   ///< floor stack, top link
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
        QVector3D casterPos;        ///< camera position
        int casterColumn;           ///< frame column
        float casterRayX;           ///< ray direction, X
        float casterRayZ;           ///< ray direction, Z
        float casterPC;             ///< 1 / cos(column pan), lens correction
        float casterHorizon;        ///< horizon row
        float casterVShift;         ///< horizon shift from the tilt, in rows
        float casterProjectionX;    ///< see projectionX()
        float casterProjectionY;    ///< see projectionY()

        Strip vStrips[StripsMax];           ///< walls crossed by the ray
        uint16_t vStripsMap[StripsMax];     ///< vStrips indices, near to far
        int vStripsCount;                   ///< in vStrips

        Strip vStripsClipped[StripsMax];    ///< strips to draw, clipped to their rows
        int vStripsClippedCount;            ///< in vStripsClipped
    } Context;

    /// \brief Wall geometry cached by wallsMash()
    typedef struct {
        float beginX, beginZ;       ///< start node position
        float endX, endZ;           ///< end node position
        float deltaX, deltaZ;       ///< end - begin
        float length;               ///< in world units
        float invLen;               ///< 1 / length
    } Segment;

// Contexts
    Context renderStates[WorkerPool::WorkersMax];   ///< one per worker
    Context probeState;                             ///< for probeLevels()

// Frames
    int frameResoX;                         ///< in pixels
    int frameResoY;                         ///< in pixels
    int glowmapReso;                        ///< cells per side
    std::atomic<int> pendingFrameResoX;     ///< resize requested by setFramesReso()
    std::atomic<int> pendingFrameResoY;     ///< same
    std::atomic<int> pendingGlowmapReso;    ///< resize requested by setGlowmapReso()
    Viewpoint viewPoint;                    ///< camera of the current render()

    QImage * image;             ///< owns the frame pixels
    uint32_t * frame;           ///< image pixels
    uint32_t * frameLast;       ///< previous frame, for the motion blur
    bool framesAllocated;       ///< frame buffers ready

// Scene, prepared each render()
    Segment * segments;         ///< one per wall

    uint32_t floorRay;          ///< sun ray color on the floors
    uint32_t ceilingRay;        ///< sun ray color on the ceilings

    __m128 sunAmbientArgb;      ///< sunAmbient * strength
    __m128 sunRayColorArgb;     ///< sunRayColor * strength

    float fogDistanceInv;       ///< 1 / fog length
    float fogDistanceOffset;    ///< distance of full fog

// Wall picking
    int clickX;                 ///< frame column to pick
    int clickY;                 ///< frame row to pick
    bool clickRegistered;       ///< pick pending
    uint16_t clickWallID;       ///< picked wall, ClickNoWall if none

// Glowmap
    float glowmapArea;          ///< world units per side
    float glowmapScale;         ///< cells per world unit
    float glowmapInvScale;      ///< world units per cell
    __m128 * glowmap;           ///< still and moving lights
    __m128 * glowmapStill;      ///< still lights only
    __m128 glowBleed;           ///< GlowBleedFactor in each lane

    QList<BoundingBox> glowmapDirtyBoxes;   ///< cells to redraw this frame
    QList<BoundingBox> glowmapDirtyLast;    ///< cells reached by the moving lights last frame

    float * panTable;           ///< pan angle per column, lens corrected

// Allocation
    /// \brief Empty object pools of the given sizes
    void allocateObjects(int noNodes, int noWalls, int noTextures, int noLights);
    /// \brief Frame buffers and panTable
    void allocateFrames(int width, int height);
    /// \brief Zeroed pixel buffer, false if out of memory
    bool allocatePixelBuffer(uint32_t *& pixels, int width, int height);
    /// \brief Fill panTable
    void buildTables();
    /// \brief Pixels per unit of tan(angle), across the columns
    float projectionX() const;
    /// \brief Pixels per unit of tan(angle), across the rows
    float projectionY() const;
    /// \brief Glowmaps, the still lights redrawn
    void allocateGlowmap(int size);
    /// \brief Free everything
    void deallocate();

// Scene preparation
    /// \brief Fog constants
    void fogSunMash();
    /// \brief Segments and wall sun rays
    void wallsMash();
    /// \brief Floor / ceiling sun rays, lights and glowmap
    void lightsMash();
    /// \brief World to glowmap cell coordinates
    void sceneGetCoords(const float x, const float z, float & xs, float & zs);
    /// \brief Glowmap cell to world coordinates
    void worldGetCoords(const float xs, const float zs, float & x, float & z);
    /// \brief Draw the still or moving lights into the glowmap, on the worker pool
    void glowmapConcurrent(__m128 * gm, bool still);
    /// \brief Same, glowmap rows [z1, z2)
    void glowmapChunk(__m128 * gm, bool still, int z1, int z2);

// Column rendering
    /// \brief Render the columns on the worker pool
    void renderConcurrent();
    /// \brief Render the columns [x1, x2)
    void renderChunk(Context & state, int x1, int x2);
    /// \brief Find the walls crossed by the column ray, sorted near to far
    void castStrips(Context & state);
    /// \brief Clip the walls and surfaces of a column through the apertures, from strip depth on
    void renderVertical(Context & state, int depth, Aperture ap);
    /// \brief Floor of a wall side along the ray
    Cap2D capFloorHeight(const Context & state, const Strip & strip, bool back);
    /// \brief Ceiling of a wall side along the ray
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
    /// \brief Draw one wall strip
    void vstripDraw(Context & state, const Strip & strip);
    /// \brief Lighting terms of a surface
    static SurfaceLighting surfaceLighting(uint16_t surfaceFlags, __m128 ambient, __m128 ray, __m128 bleed, bool sunBleed);
    /// \brief Bilinear glowmap sample, at cell coordinates
    __m128 sampleGlowmap(float glowX, float glowZ);
    /// \brief SSE flooring
    static int floorToInt(float x);
    /// \brief Index of the highest set bit (x > 0)
    static int highestBit(uint32_t x);

// Probing
    /// \brief Floor / ceiling plane of a wall side, in world space
    Cap3D capPlane(const Context & state, const Strip & strip, bool back, bool ceiling);

// Config (die.ini)
    /// \brief Default settings
    void configInit();
    /// \brief Read the settings, false if the file cannot be opened
    bool configLoad(const QString & filename);
    /// \brief Write the settings, false if the file cannot be opened
    bool configSave(const QString & filename);
};

/// \brief The engine renderer
extern Renderer renderer;

#endif // RENDERER_H
