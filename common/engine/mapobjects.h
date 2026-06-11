/**
    DIE ENGINE
    Depth Integration Engine / A modern ray-caster
    (c) Fred's Lab 2024-2026
    Frédéric Meslin / info@fredslab.net
    SPDX-License-Identifier: MIT
    If used commercially, contributions, donations are highly appreciated.

    map objects
*/

#ifndef MAPOBJECTS_H
#define MAPOBJECTS_H

#include <QVector3D>
#include <stdint.h>
#include "easings.h"

static constexpr int OBJECT_NAME_MAX = 31;
static constexpr int OBJECT_PATH_MAX = 127;
static constexpr int PATH_NODES_MAX  = 24;

/*****************************************************************************/
typedef enum : uint16_t {
    NODE_FLAG_FREE          = 0x0000,
    NODE_FLAG_USED          = 0x0001,
} NODE_FLAGS;

/**
    \brief Map node: a 3D position shared by the map objects
*/
typedef struct {
    QVector3D pos;
    uint16_t flags;
    uint16_t tag;
    float metaA;            ///< generic per-node metadata
    float metaB;            ///< generic per-node metadata
    float metaC;            ///< generic per-node metadata
    bool selected;
} Node;

/*****************************************************************************/
/**
    \brief Texture mapping of one face of a map object
*/
typedef struct {
    uint16_t id;
    float scaleX;
    float scaleY;
    float shiftX;
    float shiftY;
    uint16_t flags;
} Surface;

/*****************************************************************************/
typedef enum : uint16_t {
    WALL_SURFACE_FRONT = 0,
    WALL_SURFACE_BACK,
    WALL_SURFACE_FLOOR,
    WALL_SURFACE_CEILING,
    WALL_SURFACES_COUNT,
} WALL_SURFACES;

typedef enum : uint16_t {
    WALL_FLAG_FREE          = 0x0000,
    WALL_FLAG_INVISIBLE     = 0x0001,
    WALL_FLAG_BACKCULLED    = 0x0002,
    WALL_FLAG_ALPHA         = 0x0004,
    WALL_FLAG_NO_SHADOW     = 0x0008,
    WALL_FLAG_FLOOR_FRONT   = 0x0010,
    WALL_FLAG_FLOOR_BACK    = 0x0020,
    WALL_FLAG_CEILING_FRONT = 0x0040,
    WALL_FLAG_CEILING_BACK  = 0x0080,
    WALL_FLAG_HIGHLIGHTED   = 0x1000,
} WALL_FLAGS;

/**
    \brief Vertical wall between two nodes, with floor / ceiling extensions
*/
typedef struct {
    uint16_t nodeID1;
    uint16_t nodeID2;
    float height;

    QVector2D dir;          ///< computed geometry, see Map::computeWall()
    QVector2D normal;       ///< computed geometry, see Map::computeWall()
    float length;           ///< computed geometry, see Map::computeWall()

    Surface surfaces[WALL_SURFACES_COUNT];
    uint16_t flags;
    bool selected;
} Wall;

/*****************************************************************************/
/**
    \brief Nested map instance, loaded from a separate file
*/
typedef struct {
    uint16_t nodeID;
    char name[OBJECT_NAME_MAX + 1];
    char path[OBJECT_PATH_MAX + 1];

    float pan;
    float scale;

    uint16_t tag;
    uint16_t flags;
    bool selected;
} Submap;

/*****************************************************************************/
typedef enum : uint16_t {
    DOOR_MODE_PIVOT = 0,
    DOOR_MODE_LATERAL,
    DOOR_MODE_VERTICAL,

    DOOR_MODES_COUNT,
} DOOR_MODES;

typedef enum : uint16_t {
    DOOR_SURFACE_FRONT = 0,
    DOOR_SURFACE_BACK,
    DOOR_SURFACE_SIDE,

    DOOR_SURFACES_COUNT,
} DOOR_SURFACES;

typedef enum : uint16_t {
    DOOR_FLAG_FREE          = 0x0000,
    DOOR_FLAG_ALPHA         = 0x0004,
    DOOR_FLAG_OPENING       = 0x1000,
    DOOR_FLAG_CLOSING       = 0x2000,
    DOOR_FLAG_SHAKING       = 0x4000,
    DOOR_FLAG_LOCKED        = 0x8000,
} DOOR_FLAGS;

/**
    \brief Animated door (pivot, lateral or vertical)
*/
typedef struct {
    uint16_t nodeID;
    char name[OBJECT_NAME_MAX + 1];

    float width;
    float height;
    float thick;
    float angle;
    float swing;
    float time;
    uint16_t mode;          ///< one of DOOR_MODES
    uint16_t easing;        ///< one of EASING_TYPES

    float pan;              ///< animation state (0.0 closed .. 1.0 open)
    float shake;            ///< animation state

    Surface surfaces[DOOR_SURFACES_COUNT];
    uint16_t tag;
    uint16_t flags;
    bool selected;
} Door;

/*****************************************************************************/
typedef enum : uint16_t {
    LIFT_MODE_Y_AXIS = 0,
    LIFT_MODE_X_AXIS,
    LIFT_MODE_Z_AXIS,
    LIFT_MODE_PATH,
    LIFT_MODE_LOOP,
    LIFT_MODES_COUNT,
} LIFT_MODES;

typedef enum : uint16_t {
    LIFT_SURFACE_TOP = 0,
    LIFT_SURFACE_BOTTOM,
    LIFT_SURFACE_SIDES,
    LIFT_SURFACES_COUNT,
} LIFT_SURFACES;

typedef enum : uint16_t {
    LIFT_FLAG_FREE       = 0x0000,
    LIFT_FLAG_ALPHA      = 0x0004,
    LIFT_FLAG_HALTABLE   = 0x0010,
    LIFT_FLAG_CONTINUOUS = 0x0020,
    LIFT_FLAG_RETURN     = 0x0040,
    LIFT_FLAG_GOING      = 0x0100,
    LIFT_FLAG_RETURNING  = 0x0200,
    LIFT_FLAG_HALTED     = 0x0400,
    LIFT_FLAG_LOCKED     = 0x8000,
} LIFT_FLAGS;

/**
    \brief Animated moving platform
*/
typedef struct {
    uint16_t nodeID;
    char name[OBJECT_NAME_MAX + 1];

    float width;
    float length;
    float thick;
    float travel;
    float time;
    uint16_t mode;          ///< one of LIFT_MODES
    uint16_t easing;        ///< one of EASING_TYPES

    float pan;              ///< animation state (0.0 .. 1.0 along the travel)

    Surface surfaces[LIFT_SURFACES_COUNT];
    uint16_t tag;
    uint16_t flags;
    bool selected;
} Lift;

/*****************************************************************************/
typedef enum : uint16_t {
    SPRITE_FLAG_FREE        = 0x0000,
    SPRITE_FLAG_INVISIBLE   = 0x0001,
    SPRITE_FLAG_BACKCULLED  = 0x0002,
    SPRITE_FLAG_AUTOPAN     = 0x0010,
    SPRITE_FLAG_SHADOWS     = 0x0020,
} SPRITE_FLAGS;

/**
    \brief Textured quad standing at a node
*/
typedef struct {
    uint16_t nodeID;
    char name[OBJECT_NAME_MAX + 1];

    float width;
    float height;
    float pan;
    Surface surface;

    uint16_t tag;
    uint16_t flags;
    bool selected;
} Sprite;

/*****************************************************************************/
typedef enum : uint16_t {
    STAIRCASE_FLAG_FREE = 0x0000,
} STAIRCASE_FLAGS;

typedef enum : uint16_t {
    STAIRCASE_SURFACE_STEPFALL = 0,
    STAIRCASE_SURFACE_STEPTOP,
    STAIRCASE_SURFACE_SIDES,
    STAIRCASE_SURFACES_COUNT,
} STAIRCASE_SURFACES;

/**
    \brief Flight of steps
*/
typedef struct {
    uint16_t nodeID;

    float pan;
    float height;
    float width;
    float length;
    uint16_t steps;
    Surface surfaces[STAIRCASE_SURFACES_COUNT];

    uint16_t flags;
    bool selected;
} Staircase;

/*****************************************************************************/
typedef enum : uint16_t {
    LIGHT_ANIM_COLOR_A = 0,
    LIGHT_ANIM_COLOR_B,
    LIGHT_ANIM_CYCLE,
    LIGHT_ANIM_PULSE,
    LIGHT_ANIM_FLASH,
    LIGHT_ANIM_FLICKER,
} LIGHT_ANIMS;

typedef enum : uint16_t {
    LIGHT_FLAG_FREE     = 0x0000,
    LIGHT_FLAG_ENABLE   = 0x0002,
} LIGHT_FLAGS;

/**
    \brief Animated point light bound to a node
*/
typedef struct {
    uint16_t nodeID;

    uint32_t colorA;
    uint32_t colorB;
    float strength;
    float speed;
    uint16_t anim;          ///< one of LIGHT_ANIMS

    uint32_t color;         ///< animation state, interpolated every frame
    float phase;            ///< animation state

    uint16_t tag;
    uint16_t flags;
    bool selected;
} Light;

/*****************************************************************************/
typedef enum : uint16_t {
    SPEAKER_FLAG_FREE      = 0x0000,
    SPEAKER_FLAG_AUTOPLAY  = 0x0001,
    SPEAKER_FLAG_TRIGGER   = 0x0002,
    SPEAKER_FLAG_TOGGLE    = 0x0004,
    SPEAKER_FLAG_LOOP      = 0x0008,
    SPEAKER_FLAG_OMNI      = 0x0010,
    SPEAKER_FLAG_PLAYING   = 0x1000,
} SPEAKER_FLAGS;

/**
    \brief Spatial sound source bound to a node
*/
typedef struct {
    uint16_t nodeID;
    char name[OBJECT_NAME_MAX + 1];
    char path[OBJECT_PATH_MAX + 1];

    float volume;
    float size;
    float pan;

    uint16_t tag;
    uint16_t flags;
    bool selected;
} Speaker;

/*****************************************************************************/
/**
    \brief Sequence of nodes used as waypoints
*/
typedef struct {
    uint16_t nodes[PATH_NODES_MAX];
    uint16_t nodesCount;

    char name[OBJECT_NAME_MAX + 1];

    float pan;

    uint16_t tag;
    uint16_t flags;
    bool selected;
} Path;

#endif // MAPOBJECTS_H
