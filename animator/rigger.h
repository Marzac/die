/**
    RIGGER
    Animation editor for the DIE engine
    (c) Fred's Lab 2024-2026
    Frédéric Meslin / info@fredslab.net
    SPDX-License-Identifier: MIT
    If used commercially, contributions, donations are highly appreciated.

    rigger model
*/

#ifndef RIGGER_H
#define RIGGER_H

#include "rigobjects.h"
#include "rig.h"
#include "primitives.h"

#include <QVector2D>
#include <QVector3D>
#include <QList>
#include <QImage>

#include <functional>
#include <stdint.h>

typedef enum {
    RIG_MODE_JOINTS = 0,
    RIG_MODE_BONES,
    RIG_MODE_CONFIG,
} RIG_MODES;

typedef enum : uint32_t {
    FLAG_DISPLAY_JOINTS = 0x0001,
    FLAG_DISPLAY_BONES  = 0x0002,
    FLAG_DISPLAY_FLESH  = 0x0004,
    FLAGS_DEFAULT = FLAG_DISPLAY_JOINTS | FLAG_DISPLAY_BONES | FLAG_DISPLAY_FLESH,
} RIG_FLAGS;

/*****************************************************************************/
constexpr int RIGGER_JOINT_RADIUS = 8;
constexpr int RIGGER_BONE_RADIUS = 16;

/// \brief Pixels spanned by the view diameter (sets the world-to-screen scale)
constexpr float RIGGER_VIEW_SCALE = 256.0f;

/*****************************************************************************/
class Rigger
{
public:
    Rigger();
    void init();
    void terminate();

    RIG_MODES mode;
    uint32_t flags;

    bool editAllFrames;
    ViewpointOrtho rigView;

    /// \brief World-to-screen scale, derived from the view diameter
    float zoom() const {
        return rigView.diameter > 0.0f ? RIGGER_VIEW_SCALE / rigView.diameter : 1.0f;
    }

    Rig rig;

    int selectedJoint;
    int selectedBone;
    uint16_t selectedTextureID;

    void selectAll();
    void deselect();

    void cut();
    void copy();
    void paste(QVector2D pos);

// Per-object operations.
	void jointSelect(int jId);
	bool jointAdd(QVector3D pos, int & jId);
	void jointDelete(int jId);
	void jointSelectAll();
	void jointDeselectAll();
	bool jointFindInCircle(QVector2D pos, int & jId);
	bool jointFindInRect(QVector2D c1, QVector2D c2, int & jId);

	void boneSelect(int bId);
	bool boneAdd(int j1, int j2, int & bId);
	void boneDelete(int bId);
	void boneSelectAll();
	void boneDeselectAll();
	bool boneFindInCircle(QVector2D pos, int & bId);
	bool boneFindInRect(QVector2D c1, QVector2D c2, int & bId);

    /// \brief Project a world position onto the 2D cylinder view plane
    QVector2D to2D(const QVector3D & pos) const;

    /**
        \brief Render the bone flesh of a single pose into an image

        The image is cleared to transparent first. \p org / \p zoom map world
        units to image pixels, the same way the live editor view does; \p joints
        is the pose to render (indexed the same way as the bones' jointID1/2).
    */
    void renderFlesh(QImage & image, const QVector2D & org, float zoom, const QList<Joint> & joints);

    /**
        \brief Render an interpolated pose of an animation, fitted to its
               bounding box, into an image (e.g. for sprite sheet baking)

        \param image destination, cleared to transparent and entirely filled
               by the animation's (padded) bounding box
        \param animationId index into rig.animations
        \param frameCursor frame position; interpolates between the floor and
               ceiling neighbour frames, wrapping across the animation's loop
    */
    void renderAnimationFrame(QImage & image, int animationId, float frameCursor);
};

extern Rigger rigger;

#endif // RIGGER_H
