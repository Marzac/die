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
#include "drawer.h"

#include <algorithm>
#include <stdint.h>
#include <math.h>

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
    rig.load("lastrig.rig");    // reload the last session if present (keeps the default otherwise)

    mode = RIG_MODE_JOINTS;
    flags = FLAG_DISPLAY_JOINTS | FLAG_DISPLAY_BONES;
    editAllFrames = true;
    rigView = { 0.0f, 32.0f, 0.0f };
    selectedTextureID = 0;

    deselect();
}

void Rigger::terminate()
{
    rig.save("lastrig.rig");    // remember this session for the next launch
    rig.terminate();
}

/*****************************************************************************/
void Rigger::selectAll()
{
    switch (mode) {
        case RIG_MODE_JOINTS: jointSelectAll(); break;
        case RIG_MODE_BONES: boneSelectAll();  break;
        default: break;
    }
}

void Rigger::deselect()
{
    selectedJoint = RIG_UNSELECTED;
    selectedBone = RIG_UNSELECTED;
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
    mode = RIG_MODE_JOINTS;
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
    mode = RIG_MODE_JOINTS;
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

    if (selectedJoint == jId) selectedJoint = RIG_UNSELECTED;
    else if (selectedJoint > jId) selectedJoint--;
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
    mode = RIG_MODE_BONES;
}

bool Rigger::boneAdd(int j1, int j2, int & bId)
{
    Frame * cur = rig.currentFramePtr();
    if (!cur) return false;
    if (j1 == j2) return false;
    if (j1 < 0 || j1 >= cur->joints.count()) return false;
    if (j2 < 0 || j2 >= cur->joints.count()) return false;

    Bone b{};
    b.jointID1 = (uint16_t) j1;
    b.jointID2 = (uint16_t) j2;
    b.width = 8.0f;
    b.length = 0.0f;
    b.offset = 0.0f;
    b.minWidth = 1.0f;
    b.imageCount = 0;
    b.selected = true;

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

/*****************************************************************************/
void Rigger::renderFlesh(QImage & image, const QVector2D & org, float zoom, const QList<Joint> & joints)
{
    image.fill(Qt::transparent);

    QImage & src = rig.textures;
    if (src.isNull()) return;

    QImage strip = src.format() == QImage::Format_ARGB32 ? src
                 : src.convertToFormat(QImage::Format_ARGB32);
    int size = strip.width();
    if (size <= 0) return;
    int count = strip.height() / size;
    if (count <= 0) return;

    Texture texture;
    texture.pixels = reinterpret_cast<uint32_t *>(strip.bits());
    texture.size   = (uint16_t) size;
    texture.count  = (uint16_t) count;
    texture.mask   = (uint16_t) (size - 1);
    texture.block  = (uint32_t) (size * size);

    for (int i = 0; i < rig.bones.count(); i++) {
        const Bone & b = rig.bones[i];
        if (b.flags & BONE_FLAG_INVISIBLE) continue;
        if (b.imageCount == 0) continue;
        if (b.jointID1 >= joints.count()) continue;
        if (b.jointID2 >= joints.count()) continue;

        QVector2D p1 = org + to2D(joints[b.jointID1].pos) * zoom;
        QVector2D p2 = org + to2D(joints[b.jointID2].pos) * zoom;

        QVector2D axis = p2 - p1;
        float len = axis.length();
        QVector2D dir  = len > 0.0001f ? axis / len : QVector2D(0.0f, -1.0f);
        QVector2D perp = QVector2D(-dir.y(), dir.x());

        QVector2D mid = (p1 + p2) * 0.5f + dir * (b.offset * zoom);
        float halfLen = (len + b.length * zoom) * 0.5f;
        float halfWid = (b.width * zoom) * 0.5f;

    // Rotate: the flat quad foreshortens with the view angle (edge-on at 90 deg).
    // minWidth floors the magnitude so the texture never fully vanishes,
    // while the sign of cos still flips the quad past the 90 deg mark.
        if (b.flags & BONE_FLAG_ROTATE) {
            float c = cosf(rigView.pan * D2R);
            float w = halfWid * c;
            float minHalf = (b.minWidth * zoom) * 0.5f;
            if (fabsf(w) < minHalf)
                w = copysignf(minHalf, c != 0.0f ? c : 1.0f);
            halfWid = w;
        }

        QVector2D c0 = mid - dir * halfLen - perp * halfWid;
        QVector2D c1 = mid + dir * halfLen - perp * halfWid;
        QVector2D c2 = mid + dir * halfLen + perp * halfWid;
        QVector2D c3 = mid - dir * halfLen + perp * halfWid;

    // Pick the arc image facing the current view angle
        float step = 360.0f / b.imageCount;
        int arc = ((int) roundf(rigView.pan / step)) % b.imageCount;
        if (arc < 0) arc += b.imageCount;
        uint16_t surfaceId = b.images[arc];
        if (surfaceId >= count) continue;

    // Mirror: flip the texture across its width axis (the horizontal of the image)
        float v0 = (b.flags & BONE_FLAG_MIRROR) ? 1.0f : 0.0f;
        float v1 = (b.flags & BONE_FLAG_MIRROR) ? 0.0f : 1.0f;

    // Normalised UVs: u runs along the bone (length), v across it (width)
        Quad q;
        q.xs[0] = c0.x(); q.ys[0] = c0.y(); q.us[0] = 0.0f; q.vs[0] = v0;
        q.xs[1] = c1.x(); q.ys[1] = c1.y(); q.us[1] = 1.0f; q.vs[1] = v0;
        q.xs[2] = c2.x(); q.ys[2] = c2.y(); q.us[2] = 1.0f; q.vs[2] = v1;
        q.xs[3] = c3.x(); q.ys[3] = c3.y(); q.us[3] = 0.0f; q.vs[3] = v1;
        q.surfaceId = surfaceId;

        drawerQuad(image, &q, texture);
    }
}

/*****************************************************************************/
void Rigger::renderAnimationFrame(QImage & image, int animationId, float frameCursor)
{
    if (animationId < 0 || animationId >= rig.animations.count()) {
        image.fill(Qt::transparent);
        return;
    }

    Animation & a = rig.animations[animationId];
    int count = a.frames.count();
    if (count == 0) {
        image.fill(Qt::transparent);
        return;
    }

// Wrap the cursor into [0, count) and split into the two neighbour frames
    float cursor = fmodf(frameCursor, (float) count);
    if (cursor < 0.0f) cursor += (float) count;
    int f0 = (int) floorf(cursor);
    int f1 = (f0 + 1) % count;
    float t = cursor - (float) f0;

    const Frame & frameA = a.frames[f0];
    const Frame & frameB = a.frames[f1];
    int jointCount = std::min(frameA.joints.count(), frameB.joints.count());

// Interpolated pose; flags/selection do not matter for rendering, only pos
    QList<Joint> pose;
    pose.reserve(jointCount);
    for (int i = 0; i < jointCount; i++) {
        Joint j = frameA.joints[i];
        j.pos = frameA.joints[i].pos * (1.0f - t) + frameB.joints[i].pos * t;
        pose.append(j);
    }

// Bounding box across every frame of the animation (same projection angle),
// so every baked frame of the sheet shares the same framing
    QVector2D bbMin(1e9f, 1e9f), bbMax(-1e9f, -1e9f);
    for (const Frame & f : a.frames) {
        for (const Joint & j : f.joints) {
            QVector2D p = to2D(j.pos);
            bbMin.setX(std::min(bbMin.x(), p.x()));
            bbMin.setY(std::min(bbMin.y(), p.y()));
            bbMax.setX(std::max(bbMax.x(), p.x()));
            bbMax.setY(std::max(bbMax.y(), p.y()));
        }
    }

// Pad by the widest bone reach so the flesh quads are not clipped at the edges
    float pad = 0.0f;
    for (const Bone & b : rig.bones)
        pad = std::max(pad, b.width * 0.5f + b.length * 0.5f + fabsf(b.offset));
    bbMin -= QVector2D(pad, pad);
    bbMax += QVector2D(pad, pad);

    QVector2D bbSize = bbMax - bbMin;
    if (bbSize.x() <= 0.0f || bbSize.y() <= 0.0f) {
        image.fill(Qt::transparent);
        return;
    }

    float fitZoom = std::min(image.width() / bbSize.x(), image.height() / bbSize.y());
    QVector2D bbCenter = (bbMin + bbMax) * 0.5f;
    QVector2D org = QVector2D(image.width() * 0.5f, image.height() * 0.5f) - bbCenter * fitZoom;

    renderFlesh(image, org, fitZoom, pose);
}