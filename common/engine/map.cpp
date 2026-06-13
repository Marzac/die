/**
    DIE ENGINE
    Depth Integration Engine / A modern ray-caster
    (c) Fred's Lab 2024-2026
    Frédéric Meslin / info@fredslab.net
    SPDX-License-Identifier: MIT
    If used commercially, contributions, donations are highly appreciated.

    map model
*/

#include "map.h"

#include "globals.h"
#include "renderer.h"
#include "audio.h"
#include "easings.h"
#include "tags.h"

#include <QVector3D>

#include "colors.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>
#include <cstring>

static constexpr float GEO_EPSILON =  0.0625f;

/*****************************************************************************/
// Play a positional sound, tolerate a missing audio backend
static void playSoundAt(QSpatialSound * sound, const QVector3D & pos)
{
    if (!sound) return;
    sound->setPosition(pos);
    sound->play();
}

/*****************************************************************************/
Map::Map() :
    pan(0.0f), scale(1.0f),
    sun{}, fog{}
{
    resetEnvironment();
}

void Map::resetEnvironment()
{
    sun = {};
    sun.hour = 9.0f;
    sun.angle = 1.0f;
    sun.ambientColor = 0xBBBBBB;
    sun.ambientStrength = 1.0f;
    sun.rayColor = 0xFFFFDD;
    sun.rayStrength = 1.0f;

    fog = {};
    fog.distanceNear = 8.0f;
    fog.distanceFar = 320.0f;
    fog.color = 0x000000;
    fog.flags = FOG_FLAG_ENABLE;
}

/*****************************************************************************/
void Map::init()
{
    origin = QVector3D(0.0f, 0.0f, 0.0f);
    scale = 1.0f;
    pan = 0.0f;
    textures.load(":/Textures/default.png");
    clearObjects();
}

void Map::terminate()
{
    clearObjects();
}

void Map::clearObjects()
{
    nodes.clear();
    walls.clear();
    submaps.clear();
    doors.clear();
    lifts.clear();
    sprites.clear();
    staircases.clear();
    lights.clear();
    speakers.clear();
    paths.clear();
    maps.clear();
}

/*****************************************************************************/
void Map::update()
{
    updateAllDoors();
    updateAllLifts();

// Propagate to the submaps
    for (int i = 0; i < maps.count(); i++)
        maps[i].update();
}

void Map::updateAllDoors()
{
    for (int i = 0; i < doors.count(); i++) {
        Door & d = doors[i];
        if (d.time == 0.0f) continue;
        if (d.flags & DOOR_FLAG_OPENING) {
            d.pan += dt / d.time;
            if (d.pan >= 1.0f) {
                d.pan = 1.0f;
                d.flags &= ~DOOR_FLAG_OPENING;
            }

        }else if (d.flags & DOOR_FLAG_CLOSING) {
            d.pan -= dt / d.time;
            if (d.pan <= 0.0f) {
                d.pan = 0.0f;
                d.flags &= ~DOOR_FLAG_CLOSING;
                playSoundAt(audio.soundDoorClose, nodes[d.nodeID].pos);
            }

        }else if (d.flags & DOOR_FLAG_SHAKING) {
            d.shake += 1.5f * dt;
            if (d.shake >= 1.0f) {
                d.shake = 0.0f;
                d.flags &= ~DOOR_FLAG_SHAKING;
            }
        }
    }
}

void Map::updateAllLifts()
{
    for (int i = 0; i < lifts.count(); i++) {
        Lift & l = lifts[i];
        if (l.time == 0.0f) continue;
        if (l.flags & LIFT_FLAG_HALTED) continue;

        if (l.flags & LIFT_FLAG_GOING) {
            l.pan += dt / l.time;
            if (l.pan >= 1.0f) {
                l.pan = 1.0f;
                l.flags &= ~LIFT_FLAG_GOING;
                // Auto-return: continuous loops back, RETURN does a single round-trip
                if (l.flags & (LIFT_FLAG_CONTINUOUS | LIFT_FLAG_RETURN))
                    l.flags |= LIFT_FLAG_RETURNING;
            }

        } else if (l.flags & LIFT_FLAG_RETURNING) {
            l.pan -= dt / l.time;
            if (l.pan <= 0.0f) {
                l.pan = 0.0f;
                l.flags &= ~LIFT_FLAG_RETURNING;
                // Continuous loops back immediately; RETURN stops here (cycle complete)
                if (l.flags & LIFT_FLAG_CONTINUOUS)
                    l.flags |= LIFT_FLAG_GOING;
            }
        }
    }
}

/*****************************************************************************/
void Map::performAction(const QVector3D & pos)
{
    constexpr float doorTriggerRadius2 = 24.0f * 24.0f;
    constexpr float speakerTriggerRadius2 = 24.0f * 24.0f;

    for (int i = 0; i < maps.count(); i++)
        maps[i].performAction(pos);

// Scan for the doors
    int dID = -1;
    float dist = doorTriggerRadius2;
    for (int i = 0; i < doors.count(); i++) {
        Door & d = doors[i];
        Node & n = nodes[d.nodeID];
        QVector3D dp = n.pos - pos;
        float d2 = dp.lengthSquared();
        if (d2 > dist) continue;
        dist = d2; dID = i;
    }

    if (dID >= 0) {
        Door & d = doors[dID];
        if (d.flags & DOOR_FLAG_LOCKED) {
            doorShake(dID);
            playSoundAt(audio.soundDoorLocked, nodes[d.nodeID].pos);

        }else if (d.pan == 1.0f) {
            doorClose(dID);

        }else if (d.pan == 0.0f) {
            doorOpen(dID);
        }
        return;
    }

// Scan for the speakers
    int aID = -1;
    dist = speakerTriggerRadius2;
    for (int i = 0; i < speakers.count(); i++) {
        Speaker & a = speakers[i];
        Node & n = nodes[a.nodeID];
        QVector3D dp = n.pos - pos;
        float d2 = dp.lengthSquared();
        if (d2 > dist) continue;
        dist = d2; aID = i;
    }

    if (aID >= 0) {
        Speaker & a = speakers[aID];
        QSpatialSound * s = sounds[aID];
        if (!s) return;
        if (a.flags & SPEAKER_FLAG_TRIGGER) {
            if ((a.flags & SPEAKER_FLAG_TOGGLE) &&
                (a.flags & SPEAKER_FLAG_PLAYING)) {
                s->stop();
                a.flags &= ~SPEAKER_FLAG_PLAYING;

            }else {
                s->play();
                a.flags |= SPEAKER_FLAG_PLAYING;
            }
        }
    }
}

/*****************************************************************************/
void Map::doorOpen(uint16_t dId)
{
    if (dId >= doors.count()) return;
    Door & d = doors[dId];
    d.flags |= DOOR_FLAG_OPENING;
    playSoundAt(audio.soundDoorOpen, nodes[d.nodeID].pos);
}

void Map::doorClose(uint16_t dId)
{
    if (dId >= doors.count()) return;
    Door & d = doors[dId];
    d.flags |= DOOR_FLAG_CLOSING;
}

void Map::doorShake(uint16_t dId)
{
    if (dId >= doors.count()) return;
    Door & d = doors[dId];
    d.flags |= DOOR_FLAG_SHAKING;
}

/*****************************************************************************/
void Map::liftStart(uint16_t eId)
{
    if (eId >= lifts.count()) return;
    Lift & l = lifts[eId];

    if (l.flags & LIFT_FLAG_HALTED) {
        l.flags &= ~LIFT_FLAG_HALTED;
        return;
    }

    if (l.flags & (LIFT_FLAG_GOING | LIFT_FLAG_RETURNING)) return;

    if (l.pan >= 1.0f) l.flags |= LIFT_FLAG_RETURNING;
    else l.flags |= LIFT_FLAG_GOING;
}

void Map::liftStop(uint16_t eId)
{
    if (eId >= lifts.count()) return;
    Lift & l = lifts[eId];
    if (!(l.flags & LIFT_FLAG_HALTABLE)) return;

    l.flags |= LIFT_FLAG_HALTED;
}

/*****************************************************************************/
void Map::speakerApplyFlags(uint16_t sId)
{
    if (sId >= speakers.count()) return;
    Speaker & a = speakers[sId];

    QSpatialSound * s = sounds[sId];
    if (!s) return;

    s->setPosition(nodes[a.nodeID].pos);
    s->setVolume(a.volume);
    s->setSize(a.size);
    s->setRotation(QQuaternion::fromAxisAndAngle(0.0f, 1.0f, 0.0f, a.pan));

    if (a.flags & SPEAKER_FLAG_OMNI) s->setDirectivity(0.0f);
        else s->setDirectivity(0.80f);
    if (a.flags & SPEAKER_FLAG_LOOP) s->setLoops(-1);
        else s->setLoops(1);
    if (a.flags & SPEAKER_FLAG_AUTOPLAY) s->setAutoPlay(true);
        else s->setAutoPlay(false);
}

/*****************************************************************************/
void Map::pass(const QVector3D & camPos)
{
    renderer.nodesCount = 0;
    renderer.wallsCount = 0;
    renderer.texturesCount = 0;
    renderer.lightsCount = 0;

    passAsSub(camPos);
}

void Map::passAsSub(const QVector3D & camPos)
{
// Register texture
    int textureBase = 0;
    if (!textures.isNull()) {
        auto & et = renderer.textures[renderer.texturesCount++];
        int width = textures.width();
        et.pixels = (uint32_t *) textures.constBits();
        et.size = width;
        et.count = textures.height() / width;
        textureBase = renderer.texturesCount - 1;
    }

// Pass all renderable
    int nodeBase = renderer.nodesCount;
    passNodes();
    passWalls(nodeBase, textureBase);
    passLights(nodeBase);
    passDoors(textureBase);
    passLifts(textureBase);
    passStaircases(textureBase);
    passSprites(textureBase, camPos);

// Pass any submaps
    for (int i = 0; i < maps.count(); i++) {
        Map & subMap = maps[i];
        Submap & m = submaps[i];
        subMap.origin = nodes[m.nodeID].pos;
        subMap.pan = m.pan;
        subMap.scale = m.scale;
        subMap.passAsSub(camPos);
    }
}

/*****************************************************************************/
inline QVector3D Map::getAbsCoords(const QVector3D & pos)
{
    return origin + rotateAroundY(pos * scale, pan);
}

/*****************************************************************************/
void Map::passNodes()
{
    for (int i = 0; i < nodes.count(); i++) {
        Node & n = nodes[i];
        if (renderer.nodesCount == renderer.nodesAllocated) break;
        auto & en = renderer.nodes[renderer.nodesCount++];
        en.pos = getAbsCoords(n.pos);
        en.flags = n.flags;
    }
}

void Map::passWalls(int nodeBase, int textureBase)
{
    if (scale == 0.0f) return;

    auto & et = renderer.textures[textureBase];
    uint16_t maxId = et.count ? (uint16_t)(et.count - 1) : 0;
    for (int i = 0; i < walls.count(); i++) {
        Wall & w = walls[i];
        if (renderer.wallsCount == renderer.wallsAllocated) break;

        auto & ew = renderer.walls[renderer.wallsCount++];
        ew.nodeID1 = w.nodeID1 + nodeBase;
        ew.nodeID2 = w.nodeID2 + nodeBase;
        ew.height = w.height * scale;
        ew.offset = -origin;

        ew.textureID = textureBase;
        memcpy(ew.surfaces, w.surfaces, sizeof(w.surfaces));
        for (int s = 0; s < WALL_SURFACES_COUNT; s++) {
            uint16_t  id = w.surfaces[s].id;
            ew.surfaces[s].id = qMin(id, maxId);
            ew.surfaces[s].scaleX *= 1.0f / scale;
            ew.surfaces[s].scaleY *= 1.0f / scale;
        }

        ew.flags = w.flags;
        if (w.selected) ew.flags |= WALL_FLAG_HIGHLIGHTED;
    }
}

void Map::passDoors(int textureBase)
{
    for (int i = 0; i < doors.count(); i++) {
        Door & d = doors[i];

        if (renderer.nodesCount + 4 >= renderer.nodesAllocated) break;
        if (renderer.wallsCount + 4 >= renderer.wallsAllocated) break;

        if (d.height <= 0.0f) continue;
        if (d.width  <= 0.0f) continue;
        if (d.thick  <= 0.0f) continue;

        float pan = d.angle + d.swing * applyEasing(d.easing, d.pan);
        pan += sinf(4.0f * d.shake * M_PI) * 3.0f;

        float a = pan * D2R;
        QVector3D vl(cosf(a), 0.0f, -sinf(a));
        QVector3D vf(vl.z(), 0.0f, -vl.x());

        Node & nref = nodes[d.nodeID];
        QVector3D p0 = nref.pos - vf * d.thick + QVector3D(0.0f, GEO_EPSILON, 0.0f);
        QVector3D p1 = nref.pos + vf * d.thick + QVector3D(0.0f, GEO_EPSILON, 0.0f);

        renderer.nodes[renderer.nodesCount + 0].pos = getAbsCoords(p0);
        renderer.nodes[renderer.nodesCount + 1].pos = getAbsCoords(p1);
        renderer.nodes[renderer.nodesCount + 2].pos = getAbsCoords(p1 + vl * d.width);
        renderer.nodes[renderer.nodesCount + 3].pos = getAbsCoords(p0 + vl * d.width);

        for (int j = 0; j < 4; j++) {
            auto & ew = renderer.walls[renderer.wallsCount + j];
            ew.nodeID1 = renderer.nodesCount + j;
            ew.nodeID2 = renderer.nodesCount + ((j + 1) & 3);
            ew.height  = (d.height - 2.0f * GEO_EPSILON) * scale;

            ew.textureID = textureBase;
            ew.surfaces[WALL_SURFACE_FRONT] = d.surfaces[DOOR_SURFACE_FRONT];
            ew.surfaces[WALL_SURFACE_BACK]  = d.surfaces[DOOR_SURFACE_BACK];
            ew.flags = WALL_FLAG_BACKCULLED;
        }

        renderer.nodesCount += 4;
        renderer.wallsCount += 4;
    }
}

void Map::passLifts(int textureBase)
{
    for (int i = 0; i < lifts.count(); i++) {
        Lift & l = lifts[i];

        if (renderer.nodesCount + 4 >= renderer.nodesAllocated) break;
        if (renderer.wallsCount + 4 >= renderer.wallsAllocated) break;

        if (l.width  <= 0.0f) continue;
        if (l.length <= 0.0f) continue;
        if (l.thick  <= 0.0f) continue;

        Node & nref = nodes[l.nodeID];
        float hw = l.width  * 0.5f;
        float hl = l.length * 0.5f;

        float offset = applyEasing(l.easing, l.pan) * l.travel;
        QVector3D base = nref.pos;
        switch (l.mode) {
            case LIFT_MODE_Y_AXIS: base += QVector3D(0.0f, offset, 0.0f); break;
            case LIFT_MODE_X_AXIS: base += QVector3D(offset, 0.0f, 0.0f); break;
            case LIFT_MODE_Z_AXIS: base += QVector3D(0.0f, 0.0f, offset); break;
            default: break;
        }

        base += QVector3D(0.0f, GEO_EPSILON, 0.0f);
        renderer.nodes[renderer.nodesCount + 0].pos = getAbsCoords(base + QVector3D(-hw, 0.0f, -hl));
        renderer.nodes[renderer.nodesCount + 1].pos = getAbsCoords(base + QVector3D(+hw, 0.0f, -hl));
        renderer.nodes[renderer.nodesCount + 2].pos = getAbsCoords(base + QVector3D(+hw, 0.0f, +hl));
        renderer.nodes[renderer.nodesCount + 3].pos = getAbsCoords(base + QVector3D(-hw, 0.0f, +hl));

        for (int j = 0; j < 4; j++) {
            auto & ew = renderer.walls[renderer.wallsCount + j];
            ew.nodeID1 = renderer.nodesCount + j;
            ew.nodeID2 = renderer.nodesCount + ((j + 1) & 3);
            ew.height  = (l.thick - 2.0f * GEO_EPSILON) * scale;

            ew.textureID = textureBase;
            ew.surfaces[WALL_SURFACE_FRONT]   = l.surfaces[LIFT_SURFACE_SIDES];
            ew.surfaces[WALL_SURFACE_BACK]    = l.surfaces[LIFT_SURFACE_SIDES];
            ew.surfaces[WALL_SURFACE_FLOOR]   = l.surfaces[LIFT_SURFACE_BOTTOM];
            ew.surfaces[WALL_SURFACE_CEILING] = l.surfaces[LIFT_SURFACE_TOP];
            ew.flags = WALL_FLAG_FLOOR_BACK | WALL_FLAG_CEILING_BACK;
        }

        renderer.nodesCount += 4;
        renderer.wallsCount += 4;
    }
}

void Map::passSprites(int textureBase, const QVector3D & camPos)
{
    if (scale == 0.0f) return;

    auto & et = renderer.textures[textureBase];
    uint16_t maxId = et.count ? (uint16_t)(et.count - 1) : 0;
    for (int i = 0; i < sprites.count(); i++) {
        Sprite & s = sprites[i];
        if (s.flags & SPRITE_FLAG_INVISIBLE) continue;
        if (s.width <= 0.0f || s.height <= 0.0f) continue;

        if (renderer.nodesCount + 2 >= renderer.nodesAllocated) break;
        if (renderer.wallsCount + 1 >= renderer.wallsAllocated) break;

        QVector3D absCenter = getAbsCoords(nodes[s.nodeID].pos + QVector3D(0.0f, GEO_EPSILON, 0.0f));

        QVector3D vl;
        if (s.flags & SPRITE_FLAG_AUTOPAN) {
            QVector3D toCamera = camPos - absCenter;
            float a = atan2f(-toCamera.x(), -toCamera.z());
            vl = QVector3D(cosf(a), 0.0f, -sinf(a));
        } else {
            QVector3D vl_local(cosf(s.pan * D2R), 0.0f, -sinf(s.pan * D2R));
            vl = rotateAroundY(vl_local, pan);
        }

        float hw = s.width * 0.5f * scale;
        renderer.nodes[renderer.nodesCount + 0].pos = absCenter - vl * hw;
        renderer.nodes[renderer.nodesCount + 1].pos = absCenter + vl * hw;

        auto & ew = renderer.walls[renderer.wallsCount];
        ew.nodeID1 = renderer.nodesCount + 0;
        ew.nodeID2 = renderer.nodesCount + 1;
        ew.height = (s.height - 2.0f * GEO_EPSILON) * scale;
        ew.offset = -origin;
        ew.textureID = textureBase;

        Surface surf = s.surface;
        surf.id = qMin(s.surface.id, maxId);
        surf.scaleX = s.surface.scaleX * 16.0f / (s.width  * scale);
        surf.scaleY = s.surface.scaleY * 16.0f / (s.height * scale);
        ew.surfaces[WALL_SURFACE_FRONT] = surf;
        ew.surfaces[WALL_SURFACE_BACK] = surf;

        ew.flags = WALL_FLAG_ALPHA;
        if (s.flags & SPRITE_FLAG_BACKCULLED) ew.flags |= WALL_FLAG_BACKCULLED;
        if (!(s.flags & SPRITE_FLAG_SHADOWS)) ew.flags |= WALL_FLAG_NO_SHADOW;

        renderer.nodesCount += 2;
        renderer.wallsCount += 1;
    }
}

void Map::passStaircases(int textureBase)
{
    for (int i = 0; i < staircases.count(); i++) {
        Staircase & h = staircases[i];
        if (h.steps < 1) continue;
        if (h.height <= 2.0f * GEO_EPSILON || h.width <= 0.0f || h.length <= 0.0f) continue;
        float cmpHeight = h.height - 2.0f * GEO_EPSILON;

        int noNodesFloor = (h.steps + 1) * 2;  // step boundary pairs, ground level
        int noNodesFall = h.steps * 2;          // riser base pairs, one per step
        int noWallsSide = h.steps * 2;          // left + right, one per step
        int noWallsFall = h.steps;              // one riser wall per step
        int noNodes = noNodesFloor + noNodesFall;
        int noWalls = noWallsSide + noWallsFall + 1;  // +1 for back wall

        if (renderer.nodesCount + noNodes >= renderer.nodesAllocated) continue;
        if (renderer.wallsCount + noWalls >= renderer.wallsAllocated) continue;

        float a = h.pan * D2R;
        QVector3D vl(cosf(a), 0.0f, -sinf(a));
        QVector3D vf(vl.z(), 0.0f, -vl.x());

        float stepLength = h.length / (float)h.steps;
        float stepHeight = cmpHeight / (float)h.steps;
        QVector3D stepSide  = vl * h.width * 0.5f;
        QVector3D stepFront = vf * stepLength;

    // Node is at the x/z center of the staircase footprint; staircase ascends in +vf
        Node & nref = nodes[h.nodeID];
        QVector3D base = nref.pos - 0.5f * vf * h.length + QVector3D(0.0f, GEO_EPSILON, 0.0f);

        int floorBase = renderer.nodesCount;
        int wallBase = renderer.wallsCount;

    // Floor nodes — ground level, evenly spaced from back (s=0) to front (s=h.steps)
        for (int s = 0; s <= h.steps; s++) {
            QVector3D p = base + (float)s * stepFront;
            renderer.nodes[floorBase + s*2+0].pos = getAbsCoords(p - stepSide);
            renderer.nodes[floorBase + s*2+1].pos = getAbsCoords(p + stepSide);
        }
        renderer.nodesCount += noNodesFloor;

    // Side walls — left and right, increasing height per step
        for (int s = 0; s < h.steps; s++) {
            auto & ewl = renderer.walls[wallBase + s*2+0];
            auto & ewr = renderer.walls[wallBase + s*2+1];

            ewl.nodeID1 = floorBase + (s+0)*2 + 0;
            ewl.nodeID2 = floorBase + (s+1)*2 + 0;
            ewr.nodeID1 = floorBase + (s+1)*2 + 1;
            ewr.nodeID2 = floorBase + (s+0)*2 + 1;

            float height = ((float)(s + 1) * stepHeight) * scale;
            ewl.height = height;
            ewr.height = height;

            ewl.textureID = textureBase;
            ewl.surfaces[WALL_SURFACE_FRONT] = h.surfaces[STAIRCASE_SURFACE_SIDES];
            ewl.surfaces[WALL_SURFACE_CEILING] = h.surfaces[STAIRCASE_SURFACE_STEPTOP];
            ewl.surfaces[WALL_SURFACE_FLOOR] = h.surfaces[STAIRCASE_SURFACE_STEPTOP];
            ewl.flags = WALL_FLAG_BACKCULLED | WALL_FLAG_CEILING_BACK | WALL_FLAG_FLOOR_BACK;

            ewr.textureID = textureBase;
            ewr.surfaces[WALL_SURFACE_FRONT] = h.surfaces[STAIRCASE_SURFACE_SIDES];
            ewr.surfaces[WALL_SURFACE_CEILING] = h.surfaces[STAIRCASE_SURFACE_STEPTOP];
            ewr.surfaces[WALL_SURFACE_FLOOR] = h.surfaces[STAIRCASE_SURFACE_STEPTOP];
            ewr.flags = WALL_FLAG_BACKCULLED | WALL_FLAG_CEILING_BACK | WALL_FLAG_FLOOR_BACK;
        }
        renderer.wallsCount += noWallsSide;

    // Back wall — full height at the far end of the staircase
        auto & ewBack  = renderer.walls[renderer.wallsCount];
        ewBack.nodeID1 = floorBase + h.steps*2 + 0;
        ewBack.nodeID2 = floorBase + h.steps*2 + 1;
        ewBack.height = cmpHeight * scale;
        ewBack.textureID = textureBase;
        ewBack.surfaces[WALL_SURFACE_FRONT] = h.surfaces[STAIRCASE_SURFACE_SIDES];
        ewBack.surfaces[WALL_SURFACE_CEILING] = h.surfaces[STAIRCASE_SURFACE_STEPTOP];
        ewBack.surfaces[WALL_SURFACE_FLOOR] = h.surfaces[STAIRCASE_SURFACE_STEPTOP];
        ewBack.flags = WALL_FLAG_BACKCULLED | WALL_FLAG_CEILING_BACK | WALL_FLAG_FLOOR_BACK;
        renderer.wallsCount += 1;

    // Riser nodes — at the bottom of each riser, elevated s steps up
        int fallBase = renderer.nodesCount;
        for (int s = 0; s < h.steps; s++) {
            QVector3D p = base + (float)s * stepFront
                        + QVector3D(0.0f, (float)s * stepHeight, 0.0f);
            renderer.nodes[fallBase + s*2+0].pos = getAbsCoords(p - stepSide);
            renderer.nodes[fallBase + s*2+1].pos = getAbsCoords(p + stepSide);
        }
        renderer.nodesCount += noNodesFall;

    // Riser walls — step face on front, step tread on ceiling
        for (int s = 0; s < h.steps; s++) {
            auto & ew  = renderer.walls[renderer.wallsCount + s];
            ew.nodeID1 = fallBase + s*2 + 1;  // right (normal faces toward base)
            ew.nodeID2 = fallBase + s*2 + 0;  // left
            ew.height  = stepHeight * scale;

            ew.textureID = textureBase;
            ew.surfaces[WALL_SURFACE_FRONT] = h.surfaces[STAIRCASE_SURFACE_STEPFALL];
            ew.surfaces[WALL_SURFACE_CEILING] = h.surfaces[STAIRCASE_SURFACE_STEPTOP];
            ew.surfaces[WALL_SURFACE_FLOOR] = h.surfaces[STAIRCASE_SURFACE_STEPTOP];
            uint16_t riserFlags = WALL_FLAG_BACKCULLED | WALL_FLAG_CEILING_BACK;
            if (s == 0) riserFlags |= WALL_FLAG_FLOOR_BACK;
            else riserFlags |= WALL_FLAG_FLOOR_FRONT;
            ew.flags = riserFlags;
        }
        renderer.wallsCount += noWallsFall;
    }
}

/*****************************************************************************/
void Map::passLights(int nodeBase)
{
    for (int i = 0; i < lights.count(); i++) {
        if (renderer.lightsCount == renderer.lightsAllocated) return;
        Light & l = lights[i];
        if (!(l.flags & LIGHT_FLAG_ENABLE)) continue;

        uint32_t color = l.colorA;
        bool still = (l.anim == LIGHT_ANIM_COLOR_A) || (l.anim == LIGHT_ANIM_COLOR_B);

        switch (l.anim) {
        case LIGHT_ANIM_COLOR_A:
            color = l.colorA;
            break;

        case LIGHT_ANIM_COLOR_B:
            color = l.colorB;
            break;

        case LIGHT_ANIM_CYCLE: {
            l.phase = fmodf(l.phase + dt * l.speed * l.speed, 1.0f);
            float blend = 0.5f * (1.0f - cosf(2.0f * (float)M_PI * l.phase));
            color = colorsLinearSSE4(l.colorA, l.colorB, (uint16_t)(blend * 256.0f));
            break;
        }

        case LIGHT_ANIM_PULSE:
            l.phase = fmodf(l.phase + dt * l.speed * l.speed, 1.0f);
            color = (l.phase < 0.5f) ? l.colorA : l.colorB;
            break;

        case LIGHT_ANIM_FLASH: {
            l.phase = fmodf(l.phase + dt * l.speed * l.speed, 1.0f);
            float blend = easeInCubic(l.phase);
            color = colorsLinearSSE4(l.colorA, l.colorB, (uint16_t)(blend * 256.0f));
            break;
        }

        case LIGHT_ANIM_FLICKER:
            if (l.phase < 0.0f) {
                l.phase += dt;
                color = l.colorB;
            } else {
                float period = (l.speed > 0.0f) ? 1.0f / (l.speed * l.speed) : 1.0f;
                l.phase += dt;
                if (l.phase >= period) {
                    l.phase = 0.0f;
                    if ((rand() % 5) == 0)
                        l.phase = -5.0f * dt;
                }
                color = (l.phase < 0.0f) ? l.colorB : l.colorA;
            }
            break;

        default: break;
        }

        auto & el = renderer.lights[renderer.lightsCount++];
        el.nodeID = l.nodeID + nodeBase;
        el.color = color;
        el.strength = l.strength;
        el.falloff = l.falloff;
        el.still = still;
    }
}

/*****************************************************************************/
void Map::computeAllWalls()
{
    for (int i = 0; i < walls.count(); i++)
        computeWall(i);
}

void Map::computeWall(uint16_t wId)
{
    Wall & w = walls[wId];
    Node & n1 = nodes[w.nodeID1];
    Node & n2 = nodes[w.nodeID2];
    QVector3D dp = n2.pos - n1.pos;

    w.dir = QVector2D(dp.x(), dp.z());
    w.length = w.dir.length();
    if (w.length == 0.0f) {
        w.normal = QVector2D();
        return;
    }

    w.dir /= w.length;
    w.normal = QVector2D(w.dir.y(), -w.dir.x());
}

/*****************************************************************************/
QVector3D Map::rotateAroundY(const QVector3D & v, float angle)
{
    float a = angle * D2R;
    float cosA = cosf(a);
    float sinA = sinf(a);

    float x =  v.x() * cosA + v.z() * sinA;
    float y =  v.y();
    float z = -v.x() * sinA + v.z() * cosA;
    return QVector3D(x, y, z);
}

QVector2D Map::rotateAroundY(const QVector2D & v, float angle)
{
    float a = angle * D2R;
    float cosA = cosf(a);
    float sinA = sinf(a);

    float x =  v.x() * cosA + v.y() * sinA;
    float z = -v.x() * sinA + v.y() * cosA;
    return QVector2D(x, z);
}

/*****************************************************************************/
template <typename T>
static QList<T> findByTag(const QList<T> & list, const char * tagName)
{
    int tagId = tags.findByName(tagName);
    if (tagId == 0) return {};
    QList<T> result;
    for (const T & o : list)
        if (o.tag == (uint16_t)tagId) result.append(o);
    return result;
}

template <typename T>
static int findIndexByName(const QList<T> & list, const char * name)
{
    for (int i = 0; i < list.count(); i++)
        if (strcmp(list[i].name, name) == 0) return i;
    return -1;
}

/*****************************************************************************/
QList<Node>    Map::findNodesByTag(const char * tagName) const    {return findByTag(nodes, tagName);}
QList<Submap>  Map::findSubmapsByTag(const char * tagName) const  {return findByTag(submaps, tagName);}
QList<Door>    Map::findDoorsByTag(const char * tagName) const    {return findByTag(doors, tagName);}
QList<Lift>    Map::findLiftsByTag(const char * tagName) const    {return findByTag(lifts, tagName);}
QList<Sprite>  Map::findSpritesByTag(const char * tagName) const  {return findByTag(sprites, tagName);}
QList<Light>   Map::findLightsByTag(const char * tagName) const   {return findByTag(lights, tagName);}
QList<Speaker> Map::findSpeakersByTag(const char * tagName) const {return findByTag(speakers, tagName);}
QList<Path>    Map::findPathsByTag(const char * tagName) const    {return findByTag(paths, tagName);}

/*****************************************************************************/
int Map::findSubmapByName(const char * name) const  {return findIndexByName(submaps, name);}
int Map::findDoorByName(const char * name) const    {return findIndexByName(doors, name);}
int Map::findLiftByName(const char * name) const    {return findIndexByName(lifts, name);}
int Map::findSpriteByName(const char * name) const  {return findIndexByName(sprites, name);}
int Map::findSpeakerByName(const char * name) const {return findIndexByName(speakers, name);}
int Map::findPathByName(const char * name) const    {return findIndexByName(paths, name);}

/*****************************************************************************/
MapState Map::captureState() const
{
    MapState s;
    s.nodes      = nodes;
    s.walls      = walls;
    s.submaps    = submaps;
    s.doors      = doors;
    s.lifts      = lifts;
    s.sprites    = sprites;
    s.staircases = staircases;
    s.lights     = lights;
    s.speakers   = speakers;
    s.paths      = paths;
    s.sun        = sun;
    s.fog        = fog;
    return s;
}

void Map::restoreState(const MapState & s)
{
    nodes      = s.nodes;
    walls      = s.walls;
    submaps    = s.submaps;
    doors      = s.doors;
    lifts      = s.lifts;
    sprites    = s.sprites;
    staircases = s.staircases;
    lights     = s.lights;
    speakers   = s.speakers;
    paths      = s.paths;
    sun        = s.sun;
    fog        = s.fog;
}

/*****************************************************************************/
bool operator==(const MapState & a, const MapState & b)
{
    if (a.nodes      != b.nodes)      return false;
    if (a.walls      != b.walls)      return false;
    if (a.submaps    != b.submaps)    return false;
    if (a.doors      != b.doors)      return false;
    if (a.lifts      != b.lifts)      return false;
    if (a.sprites    != b.sprites)    return false;
    if (a.staircases != b.staircases) return false;
    if (a.lights     != b.lights)     return false;
    if (a.speakers   != b.speakers)   return false;
    if (a.paths      != b.paths)      return false;
    if (!(a.sun == b.sun)) return false;
    if (!(a.fog == b.fog)) return false;
    return true;
}
