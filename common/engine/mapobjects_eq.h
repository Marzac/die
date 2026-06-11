/**
    DIE ENGINE
    Depth Integration Engine / A modern ray-caster
    (c) Fred's Lab 2024-2026
    Frédéric Meslin / info@fredslab.net
    SPDX-License-Identifier: MIT
    If used commercially, contributions, donations are highly appreciated.

    map object equality operators
*/

#ifndef MAPOBJECTS_EQ_H
#define MAPOBJECTS_EQ_H

#include "mapobjects.h"
#include <cstring>

/*****************************************************************************/
static constexpr uint16_t DOOR_FLAGS_RUNTIME    = DOOR_FLAG_OPENING
                                                | DOOR_FLAG_CLOSING
                                                | DOOR_FLAG_SHAKING;

static constexpr uint16_t LIFT_FLAGS_RUNTIME    = LIFT_FLAG_GOING
                                                | LIFT_FLAG_RETURNING
                                                | LIFT_FLAG_HALTED;

static constexpr uint16_t SPEAKER_FLAGS_RUNTIME = SPEAKER_FLAG_PLAYING;

/*****************************************************************************/
inline bool operator==(const Surface& a, const Surface& b)
{
    return a.id     == b.id     &&
           a.scaleX == b.scaleX && a.scaleY == b.scaleY &&
           a.shiftX == b.shiftX && a.shiftY == b.shiftY &&
           a.flags  == b.flags;
}

inline bool operator==(const Node& a, const Node& b)
{
    return a.pos   == b.pos   &&
           a.flags == b.flags &&
           a.tag   == b.tag   &&
           a.metaA == b.metaA &&
           a.metaB == b.metaB &&
           a.metaC == b.metaC;
}

inline bool operator==(const Wall& a, const Wall& b)
{
    if (a.nodeID1 != b.nodeID1 || a.nodeID2 != b.nodeID2) return false;
    if (a.height  != b.height  || a.flags   != b.flags)   return false;
    for (int i = 0; i < WALL_SURFACES_COUNT; ++i)
        if (!(a.surfaces[i] == b.surfaces[i])) return false;
    return true;
}

inline bool operator==(const Submap& a, const Submap& b)
{
    /* selected excluded — editor-only */
    return a.nodeID == b.nodeID &&
           a.pan    == b.pan    &&
           a.scale  == b.scale  &&
           a.tag    == b.tag    &&
           a.flags  == b.flags  &&
           strcmp(a.name, b.name) == 0 &&
           strcmp(a.path, b.path) == 0;
}

inline bool operator==(const Door& a, const Door& b)
{
    if (a.nodeID != b.nodeID || a.mode   != b.mode   || a.easing != b.easing) return false;
    if (a.width  != b.width  || a.height != b.height || a.thick  != b.thick)  return false;
    if (a.angle  != b.angle  || a.swing  != b.swing  || a.time   != b.time)   return false;
    if (a.pan    != b.pan    || a.shake  != b.shake  || a.tag    != b.tag)    return false;
    if ((a.flags & ~DOOR_FLAGS_RUNTIME) != (b.flags & ~DOOR_FLAGS_RUNTIME))   return false;
    if (strcmp(a.name, b.name) != 0) return false;
    for (int i = 0; i < DOOR_SURFACES_COUNT; ++i)
        if (!(a.surfaces[i] == b.surfaces[i])) return false;
    return true;
}

inline bool operator==(const Lift& a, const Lift& b)
{
    if (a.nodeID != b.nodeID || a.mode   != b.mode   || a.easing != b.easing) return false;
    if (a.width  != b.width  || a.length != b.length || a.thick  != b.thick)  return false;
    if (a.travel != b.travel || a.time   != b.time   || a.pan    != b.pan)    return false;
    if (a.tag    != b.tag)    return false;
    if ((a.flags & ~LIFT_FLAGS_RUNTIME) != (b.flags & ~LIFT_FLAGS_RUNTIME))   return false;
    if (strcmp(a.name, b.name) != 0) return false;
    for (int i = 0; i < LIFT_SURFACES_COUNT; ++i)
        if (!(a.surfaces[i] == b.surfaces[i])) return false;
    return true;
}

inline bool operator==(const Sprite& a, const Sprite& b)
{
    return a.nodeID  == b.nodeID  &&
           a.width   == b.width   &&
           a.height  == b.height  &&
           a.pan     == b.pan     &&
           a.surface == b.surface &&
           a.tag     == b.tag     &&
           a.flags   == b.flags   &&
           strcmp(a.name, b.name) == 0;
}

inline bool operator==(const Staircase& a, const Staircase& b)
{
    if (a.nodeID != b.nodeID || a.pan    != b.pan    || a.height != b.height) return false;
    if (a.width  != b.width  || a.length != b.length || a.steps  != b.steps)  return false;
    if (a.flags  != b.flags) return false;
    for (int i = 0; i < STAIRCASE_SURFACES_COUNT; ++i)
        if (!(a.surfaces[i] == b.surfaces[i])) return false;
    return true;
}

inline bool operator==(const Light& a, const Light& b)
{
    return a.nodeID   == b.nodeID   &&
           a.colorA   == b.colorA   &&
           a.colorB   == b.colorB   &&
           a.strength == b.strength &&
           a.speed    == b.speed    &&
           a.anim     == b.anim     &&
           a.tag      == b.tag      &&
           a.flags    == b.flags;
}

inline bool operator==(const Speaker& a, const Speaker& b)
{
    return a.nodeID == b.nodeID &&
           a.volume == b.volume &&
           a.size   == b.size   &&
           a.pan    == b.pan    &&
           a.tag    == b.tag    &&
           (a.flags & ~SPEAKER_FLAGS_RUNTIME) == (b.flags & ~SPEAKER_FLAGS_RUNTIME) &&
           strcmp(a.name, b.name) == 0 &&
           strcmp(a.path, b.path) == 0;
}

inline bool operator==(const Path& a, const Path& b)
{
    if (a.nodesCount != b.nodesCount) return false;
    if (a.pan        != b.pan)        return false;
    if (a.tag        != b.tag)        return false;
    if (a.flags      != b.flags)      return false;
    if (strcmp(a.name, b.name) != 0) return false;
    for (uint16_t i = 0; i < a.nodesCount; ++i)
        if (a.nodes[i] != b.nodes[i]) return false;
    return true;
}

#endif // MAPOBJECTS_EQ_H
