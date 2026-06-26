/**
    RIGGER
    Animation editor for the DIE engine
    (c) Fred's Lab 2024-2026
    Frédéric Meslin / info@fredslab.net
    SPDX-License-Identifier: MIT
    If used commercially, contributions, donations are highly appreciated.

    rigger model
*/

#include "rigger.h"

#include <algorithm>
#include <stdint.h>
#include <string.h>
#include <math.h>

static constexpr float D2R = 3.14159265f / 180.0f;

Rigger rigger;

/*****************************************************************************/
Rigger::Rigger()
{
}

/*****************************************************************************/
void Rigger::init()
{
    rig.init();
    rig.animationAdd("idle");   // a default animation with one frame, ready to edit

    rigMode = RIG_MODE_JOINTS;
    rigView = { 0.0f, 32.0f, 0.0f };

    deselect();
}

void Rigger::terminate()
{
    rig.terminate();
}

/*****************************************************************************/
void Rigger::selectAll()
{
    switch (rigMode) {
        case RIG_MODE_JOINTS: jointSelectAll(); break;
        case RIG_MODE_BONES:  boneSelectAll();  break;
        default: break;
    }
}

void Rigger::deselect()
{
    selectedJoint = RIG_UNSELECTED;
    selectedBone  = RIG_UNSELECTED;
    jointDeselectAll();
    boneDeselectAll();
}

/*****************************************************************************/
void Rigger::cut()
{

}

void Rigger::copy()
{

}

void Rigger::paste(QVector2D pos)
{
    (void) pos;
}

/*****************************************************************************/
void Rigger::jointSelect(int jId)
{
    Frame * cur = rig.currentFramePtr();
    if (!cur) return;
    if (jId < 0 || jId >= cur->joints.count()) return;

    cur->joints[jId].selected = true;
    selectedJoint = jId;
    rigMode = RIG_MODE_JOINTS;
}

bool Rigger::jointAdd(QVector3D pos, int & jId)
{
    Frame * cur = rig.currentFramePtr();
    if (!cur) return false;

    jId = cur->joints.count();

    Joint j{};
    j.pos   = pos;
    j.apos  = pos;
    j.flags = JOINT_FLAG_USED;

// A joint belongs to the skeleton topology: add it to every frame so bones,
// which index joints, stay valid no matter which frame is edited. Only the
// current frame's copy is selected.
    for (Animation & a : rig.animations)
        for (Frame & f : a.frames) {
            Joint nj = j;
            nj.selected = (&f == cur);
            f.joints.append(nj);
        }

    selectedJoint = jId;
    rigMode = RIG_MODE_JOINTS;
    return true;
}

void Rigger::jointDelete(int jId)
{
    Frame * cur = rig.currentFramePtr();
    if (!cur) return;
    if (jId < 0 || jId >= cur->joints.count()) return;

// Remove the joint from every frame to keep the topology consistent
    for (Animation & a : rig.animations)
        for (Frame & f : a.frames)
            if (jId < f.joints.count())
                f.joints.removeAt(jId);

// Drop the bones touching the joint, reindex those past it
    for (int i = 0; i < rig.bones.count(); i++) {
        Bone & b = rig.bones[i];
        if (b.jointID1 == jId || b.jointID2 == jId) {
            boneDelete(i--);
            continue;
        }
        if (b.jointID1 > jId) b.jointID1--;
        if (b.jointID2 > jId) b.jointID2--;
    }

    if (selectedJoint == jId)      selectedJoint = RIG_UNSELECTED;
    else if (selectedJoint > jId)  selectedJoint--;
}

void Rigger::jointSelectAll()
{
    Frame * cur = rig.currentFramePtr();
    if (!cur) return;
    for (Joint & j : cur->joints) j.selected = true;
}

void Rigger::jointDeselectAll()
{
    Frame * cur = rig.currentFramePtr();
    if (!cur) return;
    for (Joint & j : cur->joints) j.selected = false;
}

bool Rigger::jointFindInCircle(QVector2D pos, int & jId)
{
    jId = RIG_UNSELECTED;
    Frame * cur = rig.currentFramePtr();
    if (!cur) return false;

    int count = cur->joints.count();
    if (count == 0) return false;

    int offset = selectedJoint < 0 ? 0 : selectedJoint + 1;
    float rMin = RIGGER_JOINT_RADIUS / zoom();

    for (int i = 0; i < count; i++) {
        int index = (i + offset) % count;
        QVector2D jp = to2D(cur->joints[index].pos);
        float r = jp.distanceToPoint(pos);
        if (r > rMin) continue;
        rMin = r;
        jId = index;
        break;
    }

    return jId != RIG_UNSELECTED;
}

bool Rigger::jointFindInRect(QVector2D c1, QVector2D c2, int & jId)
{
    jId = RIG_UNSELECTED;
    Frame * cur = rig.currentFramePtr();
    if (!cur) return false;

    for (int i = 0; i < cur->joints.count(); i++) {
        QVector2D jp = to2D(cur->joints[i].pos);
        QVector2D d1 = c1 - jp;
        QVector2D d2 = c2 - jp;
        if (d1.x() * d2.x() > 0.0f) continue;
        if (d1.y() * d2.y() > 0.0f) continue;
        cur->joints[i].selected = true;
        jId = i;
    }

    return jId != RIG_UNSELECTED;
}

/*****************************************************************************/
void Rigger::boneSelect(int bId)
{
    if (bId < 0 || bId >= rig.bones.count()) return;
    rig.bones[bId].selected = true;
    selectedBone = bId;
    rigMode = RIG_MODE_BONES;
}

bool Rigger::boneAdd(int j1, int j2, int & bId)
{
    Frame * cur = rig.currentFramePtr();
    if (!cur) return false;
    if (j1 == j2) return false;
    if (j1 < 0 || j1 >= cur->joints.count()) return false;
    if (j2 < 0 || j2 >= cur->joints.count()) return false;

    Bone b{};
    b.jointID1   = (uint16_t) j1;
    b.jointID2   = (uint16_t) j2;
    b.width      = 8.0f;
    b.length     = 0.0f;
    b.offset     = 0.0f;
    b.imageCount = 0;
    b.selected   = true;

    rig.bones.append(b);
    bId = rig.bones.count() - 1;
    return true;
}

void Rigger::boneDelete(int bId)
{
    if (bId < 0 || bId >= rig.bones.count()) return;
    rig.bones.removeAt(bId);

    if (selectedBone == bId)      selectedBone = RIG_UNSELECTED;
    else if (selectedBone > bId)  selectedBone--;
}

void Rigger::boneSelectAll()
{
    for (Bone & b : rig.bones) b.selected = true;
}

void Rigger::boneDeselectAll()
{
    for (Bone & b : rig.bones) b.selected = false;
}

bool Rigger::boneFindInCircle(QVector2D pos, int & bId)
{
    bId = RIG_UNSELECTED;
    Frame * cur = rig.currentFramePtr();
    if (!cur) return false;

    int count = rig.bones.count();
    if (count == 0) return false;

    int offset = selectedBone < 0 ? 0 : selectedBone + 1;
    float rMin = RIGGER_BONE_RADIUS / zoom();

    for (int i = 0; i < count; i++) {
        int index = (i + offset) % count;
        const Bone & b = rig.bones[index];
        if (b.jointID1 >= cur->joints.count()) continue;
        if (b.jointID2 >= cur->joints.count()) continue;

        QVector2D p1 = to2D(cur->joints[b.jointID1].pos);
        QVector2D p2 = to2D(cur->joints[b.jointID2].pos);

    // distance from the cursor to the bone segment
        QVector2D dir = p2 - p1;
        float len2 = QVector2D::dotProduct(dir, dir);
        float t = len2 > 0.0f ? QVector2D::dotProduct(pos - p1, dir) / len2 : 0.0f;
        t = std::clamp(t, 0.0f, 1.0f);
        float r = (p1 + dir * t).distanceToPoint(pos);
        if (r > rMin) continue;

        rMin = r;
        bId = index;
        break;
    }

    return bId != RIG_UNSELECTED;
}

bool Rigger::boneFindInRect(QVector2D c1, QVector2D c2, int & bId)
{
    bId = RIG_UNSELECTED;
    Frame * cur = rig.currentFramePtr();
    if (!cur) return false;

    for (int i = 0; i < rig.bones.count(); i++) {
        Bone & b = rig.bones[i];
        if (b.jointID1 >= cur->joints.count()) continue;
        if (b.jointID2 >= cur->joints.count()) continue;

        QVector2D p1 = to2D(cur->joints[b.jointID1].pos);
        QVector2D d11 = c1 - p1, d12 = c2 - p1;
        if (d11.x() * d12.x() > 0.0f) continue;
        if (d11.y() * d12.y() > 0.0f) continue;

        QVector2D p2 = to2D(cur->joints[b.jointID2].pos);
        QVector2D d21 = c1 - p2, d22 = c2 - p2;
        if (d21.x() * d22.x() > 0.0f) continue;
        if (d21.y() * d22.y() > 0.0f) continue;

        b.selected = true;
        bId = i;
    }

    return bId != RIG_UNSELECTED;
}

/*****************************************************************************/
QVector2D Rigger::to2D(const QVector3D & pos) const
{
    float a = rigView.pan * D2R;
    float x = pos.x() * cosf(a) + pos.z() * sinf(a);
    return QVector2D(x, -pos.y());
}