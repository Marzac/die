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
//#include "renderer.h"

#include <QVector2D>
#include <QVector3D>
#include <QList>

#include <functional>
#include <stdint.h>

typedef enum {
    RIG_MODE_JOINTS = 0,
    RIG_MODE_BONES,
    RIG_MODE_CONFIG,
} RIG_MODES;

/*****************************************************************************/
constexpr int RIGGER_JOINT_RADIUS = 8;
constexpr int RIGGER_BONE_RADIUS = 16;

/// \brief Pixels spanned by the view diameter (sets the world-to-screen scale)
constexpr float RIGGER_VIEW_SCALE = 256.0f;

/**
    \brief Orthographic cylinder view: the world rotated by pan about Y, seen
           from a Z distance (diameter), with a vertical offset
*/
struct RigView {
    float pan;          ///< rotation about the Y axis, in degrees
    float diameter;     ///< cylinder diameter / camera Z distance
    float y;            ///< vertical offset
};

/*****************************************************************************/
class Rigger
{
public:
    Rigger();
    void init();
    void terminate();

    RIG_MODES rigMode;
    RigView rigView;

    /// \brief World-to-screen scale, derived from the view diameter
    float zoom() const {
        return rigView.diameter > 0.0f ? RIGGER_VIEW_SCALE / rigView.diameter : 1.0f;
    }

    Rig rig;

    int selectedJoint;
    int selectedBone;

    //bool inView(const QVector3D & pos) const {
    //    return pos.y() >= viewMinY && pos.y() <= viewMaxY;
    //}

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
};

extern Rigger rigger;

#endif // RIGGER_H
