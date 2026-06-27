/**
    DIE ENGINE
    Depth Integration Engine / A modern ray-caster
    (c) Fred's Lab 2024-2026
    Frédéric Meslin / info@fredslab.net
    SPDX-License-Identifier: MIT
    If used commercially, contributions, donations are highly appreciated.

    rig objects
*/

#ifndef RIGOBJECTS_H
#define RIGOBJECTS_H

#include <QVector3D>
#include <stdint.h>

static constexpr int RIG_BONE_IMAGES_MAX = 4;   ///< Front, Right, Back, Left
static constexpr int RIG_UNSELECTED      = -1;

/*****************************************************************************/
typedef enum : uint16_t {
    JOINT_FLAG_FREE         = 0x0000,
    JOINT_FLAG_USED         = 0x0001,
} JOINT_FLAGS;

/**
    \brief Rig joint: a 3D articulation point shared by the bones
*/
typedef struct {
    QVector3D pos;          ///< rest position, as authored in the editor
    QVector3D apos;         ///< animation state, interpolated every frame

    uint16_t flags;
    bool selected;
} Joint;

/*****************************************************************************/
typedef enum : uint16_t {
    BONE_FLAG_FREE          = 0x0000,
    BONE_FLAG_INVISIBLE     = 0x0001,
    BONE_FLAG_MIRROR        = 0x0002,   ///< flip the image horizontally (left / right reuse)
    BONE_FLAG_ROTATE        = 0x0004,   ///< flesh width foreshortens with the view angle
} BONE_FLAGS;

/**
    \brief Rig bone: a camera-facing quad spanning two joints, one image per view arc
*/
typedef struct {
    uint16_t jointID1;      ///< base joint
    uint16_t jointID2;      ///< tip joint

    float width;            ///< quad width across the bone, in world units
    float length;           ///< extra quad length added to the joint span, in world units
    float offset;           ///< quad shift along the bone axis, in world units
    float minWidth;         ///< floor on the rendered width when BONE_FLAG_ROTATE foreshortens it

    uint16_t images[RIG_BONE_IMAGES_MAX];   ///< per-arc image ids: 0 Front, 1 Right, 2 Back, 3 Left
    uint16_t imageCount;    ///< number of arcs in use, from the front (0..4)

    uint16_t flags;
    bool selected;
} Bone;

#endif // RIGOBJECTS_H
