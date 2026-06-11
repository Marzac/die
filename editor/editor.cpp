/**
    WALLER
    Map editor for the DIE engine
    (c) Fred's Lab 2024-2026
    Frédéric Meslin / info@fredslab.net
    SPDX-License-Identifier: MIT
    If used commercially, contributions, donations are highly appreciated.

    editor model
*/

#include "editor.h"

#include "audio.h"

#include <algorithm>
#include <stdint.h>
#include <string.h>
#include <math.h>

Editor editor;

/*****************************************************************************/
Editor::Editor() :
    editedMap(&rootMap)
{
}

/*****************************************************************************/
void Editor::init()
{
    env.init();

    editedMap = &rootMap;
    editedMap->init();
    editedMap->load("lastmap.map");
    env.sun = editedMap->sun;
    env.fog = editedMap->fog;

    editMode = EDIT_MODE_NODES;

    selectedNode = EDIT_UNSELECTED;
    selectedWall = EDIT_UNSELECTED;
    selectedSubmap = EDIT_UNSELECTED;
    selectedDoor = EDIT_UNSELECTED;
    selectedLift = EDIT_UNSELECTED;
    selectedStaircase = EDIT_UNSELECTED;
    selectedSprite = EDIT_UNSELECTED;
    selectedLight = EDIT_UNSELECTED;
    selectedSpeaker = EDIT_UNSELECTED;
    selectedPath = EDIT_UNSELECTED;
    selectedTextureID = 0;

    gridSnap = true;
    gridSize = 8.0f;

    zoom = 1.0f;
    viewMinY = -4096.f;
    viewMaxY = 4096.f;

    gravity = false;
    collisions = false;
    wallSelector = true;
}

void Editor::terminate()
{
    editedMap->sun = env.sun;
    editedMap->fog = env.fog;
    editedMap->save("lastmap.map", false);
    editedMap->terminate();
    env.terminate();
}

/*****************************************************************************/
void Editor::selectAll()
{
    switch(editMode) {
        case EDIT_MODE_NODES:
        case EDIT_MODE_PATHS:
            nodeSelectAll();
            break;
        case EDIT_MODE_WALLS: wallSelectAll(); break;
        case EDIT_MODE_SUBMAPS: submapSelectAll(); break;
        case EDIT_MODE_DOORS: doorSelectAll(); break;
        case EDIT_MODE_LIFTS: liftSelectAll(); break;
        case EDIT_MODE_SPRITES: spriteSelectAll(); break;
        case EDIT_MODE_STAIRCASES: staircaseSelectAll(); break;
        case EDIT_MODE_LIGHTS: lightSelectAll(); break;
        case EDIT_MODE_SPEAKERS: speakerSelectAll(); break;
        default: break;
    }
}

void Editor::deselect()
{
    nodeDeselectAll();
    wallDeselectAll();
    submapDeselectAll();
    doorDeselectAll();
    liftDeselectAll();
    staircaseDeselectAll();
    spriteDeselectAll();
    lightDeselectAll();
    speakerDeselectAll();
    pathDeselectAll();
}

void Editor::clearSelection()
{
    selectedNode      = EDIT_UNSELECTED;
    selectedWall      = EDIT_UNSELECTED;
    selectedSubmap    = EDIT_UNSELECTED;
    selectedDoor      = EDIT_UNSELECTED;
    selectedLift      = EDIT_UNSELECTED;
    selectedSprite    = EDIT_UNSELECTED;
    selectedStaircase = EDIT_UNSELECTED;
    selectedLight     = EDIT_UNSELECTED;
    selectedSpeaker   = EDIT_UNSELECTED;
    selectedPath      = EDIT_UNSELECTED;
    nodeDeselectAll();
    wallDeselectAll();
    submapDeselectAll();
    doorDeselectAll();
    liftDeselectAll();
    staircaseDeselectAll();
    spriteDeselectAll();
    lightDeselectAll();
    speakerDeselectAll();
    pathDeselectAll();
}

/*****************************************************************************/
void Editor::cut()
{
// TODO!
}

void Editor::copy()
{
    copyWalls.clear();
    for (Wall & w: editedMap->walls) {
        if (!w.selected) continue;
        editedMap->nodes[w.nodeID1].selected = true;
        editedMap->nodes[w.nodeID2].selected = true;
        copyWalls.append(w);
    }

    copyNodeIDs.clear();
    copyNodes.clear();
    for (int i = 0; i < editedMap->nodes.count(); i++) {
        Node & n = editedMap->nodes[i];
        if (!n.selected) continue;
        copyNodeIDs.append(i);
        copyNodes.append(n);
    }
}

void Editor::paste(VIEW_MODES mode, QVector2D pos)
{
    nodeDeselectAll();
    wallDeselectAll();

    int nodeBase = editedMap->nodes.count();
    if (!copyWalls.isEmpty()) {
        for (Wall w: copyWalls) {
            int wnID1 = findCopiedNodeID(w.nodeID1);
            int wnID2 = findCopiedNodeID(w.nodeID2);
            if (wnID1 < 0 || wnID2 < 0) continue;
            w.nodeID1 = nodeBase + wnID1;
            w.nodeID2 = nodeBase + wnID2;
            w.selected = true;
            editedMap->walls.append(w);
        }
        selectedWall = editedMap->walls.count() - 1;
    }else selectedWall = -1;

    if (!copyNodes.isEmpty()) {
        QVector3D ref = to3D(mode, pos, QVector3D());
        QVector3D dp = ref - copyNodes.last().pos;
        for (Node n: copyNodes) {
            n.selected = true;
            n.pos += dp;
            editedMap->nodes.append(n);
        }
        selectedNode = editedMap->nodes.count() - 1;
    }else selectedNode = -1;
}

int Editor::findCopiedNodeID(uint16_t id)
{
    for (int i = 0; i < copyNodeIDs.count(); i++)
        if (copyNodeIDs[i] == id) return i;
    return -1;
}

void Editor::align()
{
    // TODO!
}

/*****************************************************************************/
void Editor::nodeSelect(int nId)
{
    if (nId >= editedMap->nodes.count()) return;
    editedMap->nodes[nId].selected = true;
    selectedNode = nId;
    if (editMode != EDIT_MODE_PATHS)
        editMode = EDIT_MODE_NODES;
}

bool Editor::nodeAdd(QVector3D pos, int & nId)
{
    Node n{};
    n.pos = pos;
    n.flags = NODE_FLAG_USED;
    n.selected = true;

    editedMap->nodes.append(n);
    nId = editedMap->nodes.count() - 1;
    return true;
}

void Editor::nodeDelete(int nId)
{
    if (nId < 0 || nId >= editedMap->nodes.count()) return;

// Delete associated walls
    for (int i = 0; i < editedMap->walls.count(); i++) {
        Wall & w = editedMap->walls[i];
        if (w.nodeID1 == nId || w.nodeID2 == nId) {
            wallDelete(i--);
            continue;
        }
        if (w.nodeID1 > nId) w.nodeID1--;
        if (w.nodeID2 > nId) w.nodeID2--;
    }

// Delete associated submaps
    for (int i = 0; i < editedMap->submaps.count(); i++) {
        Submap & m = editedMap->submaps[i];
        if (m.nodeID == nId) {
            submapDelete(i--);
            continue;
        }
        if (m.nodeID > nId) m.nodeID--;
    }

// Delete associated doors
    for (int i = 0; i < editedMap->doors.count(); i++) {
        Door & d = editedMap->doors[i];
        if (d.nodeID == nId) {
            doorDelete(i--);
            continue;
        }
        if (d.nodeID > nId) d.nodeID--;
    }

// Delete associated sprites
    for (int i = 0; i < editedMap->sprites.count(); i++) {
        Sprite & b = editedMap->sprites[i];
        if (b.nodeID == nId) {
            spriteDelete(i--);
            continue;
        }
        if (b.nodeID > nId) b.nodeID--;
    }

// Delete associated staircase
    for (int i = 0; i < editedMap->staircases.count(); i++) {
        Staircase & h = editedMap->staircases[i];
        if (h.nodeID == nId) {
            staircaseDelete(i--);
            continue;
        }
        if (h.nodeID > nId) h.nodeID--;
    }

// Delete associated lights
    for (int i = 0; i < editedMap->lights.count(); i++) {
        Light & l = editedMap->lights[i];
        if (l.nodeID == nId) {
            lightDelete(i--);
            continue;
        }
        if (l.nodeID > nId) l.nodeID--;
    }

// Delete associated lifts
    for (int i = 0; i < editedMap->lifts.count(); i++) {
        Lift & e = editedMap->lifts[i];
        if (e.nodeID == nId) {
            liftDelete(i--);
            continue;
        }
        if (e.nodeID > nId) e.nodeID--;
    }

// Delete associated speakers
    for (int i = 0; i < editedMap->speakers.count(); i++) {
        Speaker & a = editedMap->speakers[i];
        if (a.nodeID == nId) {
            speakerDelete(i--);
            continue;
        }
        if (a.nodeID > nId) a.nodeID--;
    }

// Fix up the path waypoints
    for (int i = 0; i < editedMap->paths.count(); i++) {
        Path & p = editedMap->paths[i];
        int dst = 0;
        for (int j = 0; j < p.nodesCount; j++) {
            uint16_t id = p.nodes[j];
            if (id == nId) continue;
            p.nodes[dst++] = id > nId ? id - 1 : id;
        }
        p.nodesCount = dst;
    }

    editedMap->nodes.removeAt(nId);
}

void Editor::nodeSelectAll()
{
    for (int i = 0; i < editedMap->nodes.count(); i++)
        editedMap->nodes[i].selected = true;
}

void Editor::nodeDeselectAll()
{
    for (int i = 0; i < editedMap->nodes.count(); i++)
        editedMap->nodes[i].selected = false;
}

bool Editor::nodeFindInCircle(VIEW_MODES mode, QVector2D pos, int & nId)
{
    return findInCircle<Node>(mode, pos, nId,
                            editedMap->nodes,
                            [this](const Node& n) {return static_cast<int>(&n - editedMap->nodes.data());},
                            selectedNode);
}

bool Editor::nodeFindInRect(VIEW_MODES mode, QVector2D c1, QVector2D c2, int & nId)
{
    return findInRect<Node>(mode, c1, c2, nId,
                            editedMap->nodes,
                            [this](const Node& n) {return static_cast<int>(&n - editedMap->nodes.data());});
}

/*****************************************************************************/
void Editor::wallSelect(int wId)
{
    if (wId >= editedMap->walls.count()) return;
    editedMap->walls[wId].selected = true;
    selectedWall = wId;
    editMode = EDIT_MODE_WALLS;
}

bool Editor::wallAdd(int n1, int n2, uint16_t texId, int & wId)
{
    Wall w{};
    w.nodeID1 = n1;
    w.nodeID2 = n2;
    w.height = 16.0f;
    w.flags = WALL_FLAG_BACKCULLED;

    w.surfaces[WALL_SURFACE_FRONT].id = texId;
    w.surfaces[WALL_SURFACE_FRONT].scaleX = 4.0f;
    w.surfaces[WALL_SURFACE_FRONT].scaleY = 4.0f;

    w.surfaces[WALL_SURFACE_BACK].id = texId;
    w.surfaces[WALL_SURFACE_BACK].scaleX = 4.0f;
    w.surfaces[WALL_SURFACE_BACK].scaleY = 4.0f;

    w.surfaces[WALL_SURFACE_CEILING].id = 1;
    w.surfaces[WALL_SURFACE_CEILING].scaleX = 4.0f;
    w.surfaces[WALL_SURFACE_CEILING].scaleY = 4.0f;

    w.surfaces[WALL_SURFACE_FLOOR].id = 0;
    w.surfaces[WALL_SURFACE_FLOOR].scaleX = 4.0f;
    w.surfaces[WALL_SURFACE_FLOOR].scaleY = 4.0f;

    editedMap->walls.append(w);
    wId = editedMap->walls.count() - 1;
    return true;
}

void Editor::wallDelete(int wId)
{
    if (wId < 0 || wId >= editedMap->walls.count()) return;
    editedMap->walls.removeAt(wId);
}

void Editor::wallSelectAll()
{
    for (int i = 0; i < editedMap->walls.count(); i++)
        editedMap->walls[i].selected = true;
}

void Editor::wallDeselectAll()
{
    for (int i = 0; i < editedMap->walls.count(); i++)
        editedMap->walls[i].selected = false;
}

bool Editor::wallFindInCircle(VIEW_MODES mode, QVector2D pos, int & wId)
{
    wId = -1;
    int nOff = selectedWall < 0 ? 0 : selectedWall + 1;
    int count = editedMap->walls.count();
    if (mode == VIEW_TOP) {
        float rMin = EDITOR_WALL_RADIUS / zoom;
        rMin = std::max(rMin, 0.5f * effectiveGridSize());
        for (int i = 0; i < count; i++) {
            int index = (i + nOff) % count;
            Wall & w = editedMap->walls[index];
            if (w.length == 0.0f) continue;

            Node & n1 = editedMap->nodes[w.nodeID1];
            QVector2D np = pos - QVector2D(n1.pos.x(), n1.pos.z());

            float dp = QVector2D::dotProduct(np, w.dir);
            if (dp < 0.0f || dp > w.length) continue;

            float dr = fabsf(QVector2D::dotProduct(np, w.normal));
            if (dr > rMin) continue;

            rMin = dr;
            wId = index;
            break;
        }

    }else{
        float rMin = 65536.0f;
        for (int i = 0; i < count; i++) {
            int index = (i + nOff) % count;
            Wall & w = editedMap->walls[index];
            Node & n1 = editedMap->nodes[w.nodeID1];
            Node & n2 = editedMap->nodes[w.nodeID2];
            float wBot = n1.pos.y();
            float wTop = wBot + w.height;

            QVector2D n1p = to2D(mode, n1.pos);
            QVector2D n2p = to2D(mode, n2.pos);
            n1p.setY(-wTop);
            n2p.setY(-wBot);

            QVector2D d1 = n1p - pos;
            QVector2D d2 = n2p - pos;
            if (d1.x() * d2.x() > 0.0f) continue;
            if (d1.y() * d2.y() > 0.0f) continue;

            QVector2D np = (n1p + n2p) * 0.5f;
            float r = np.distanceToPoint(pos);
            if (r > rMin) continue;

            rMin = r;
            wId = index;
            break;
        }
    }

    return wId >= 0;
}

bool Editor::wallFindInRect(VIEW_MODES mode, QVector2D c1, QVector2D c2, int & wId)
{
    wId = -1;

    for (int i = 0; i < editedMap->walls.count(); i++) {
        Wall & w = editedMap->walls[i];
        Node & n1 = editedMap->nodes[w.nodeID1];
        QVector2D np1 = to2D(mode, n1.pos);
        QVector2D n1d1 = c1 - np1;
        QVector2D n1d2 = c2 - np1;
        if (n1d1.x() * n1d2.x() > 0.0f) continue;
        if (n1d1.y() * n1d2.y() > 0.0f) continue;

        Node & n2 = editedMap->nodes[w.nodeID2];
        QVector2D np2 = to2D(mode, n2.pos);
        QVector2D n2d1 = c1 - np2;
        QVector2D n2d2 = c2 - np2;
        if (n2d1.x() * n2d2.x() > 0.0f) continue;
        if (n2d1.y() * n2d2.y() > 0.0f) continue;

        w.selected = true;
        wId = i;
    }

    return wId >= 0;
}

/*****************************************************************************/
void Editor::submapSelect(int mId)
{
    if (mId >= editedMap->submaps.count()) return;
    editedMap->submaps[mId].selected = true;
    selectedSubmap = mId;
    editMode = EDIT_MODE_SUBMAPS;
}

bool Editor::submapAdd(int node, int & mId)
{
    Submap m{};
    m.nodeID = node;
    m.pan = 0.0f;
    m.scale = 1.0f;

    editedMap->submaps.append(m);
    mId = editedMap->submaps.count() - 1;
    return true;
}

void Editor::submapDelete(int mId)
{
    if (mId < 0 || mId >= editedMap->submaps.count()) return;
    editedMap->submaps.removeAt(mId);

// maps only mirrors submaps after a loadSubmaps()
    if (mId < editedMap->maps.count())
        editedMap->maps.remove(mId);
}

void Editor::submapSelectAll()
{
    for (int i = 0; i < editedMap->submaps.count(); i++)
        editedMap->submaps[i].selected = true;
}

void Editor::submapDeselectAll()
{
    for (int i = 0; i < editedMap->submaps.count(); i++)
        editedMap->submaps[i].selected = false;
}

bool Editor::submapFindInCircle(VIEW_MODES mode, QVector2D pos, int & mId)
{
    return findInCircle<Submap>(mode, pos, mId,
        editedMap->submaps,
        [](const Submap& m) { return m.nodeID; },
        selectedSubmap);
}

bool Editor::submapFindInRect(VIEW_MODES mode, QVector2D c1, QVector2D c2, int & mId)
{
    return findInRect<Submap>(mode, c1, c2, mId,
        editedMap->submaps,
        [](const Submap& m) { return m.nodeID; });
}

/*****************************************************************************/
void Editor::doorSelect(int dId)
{
    if (dId >= editedMap->doors.count()) return;
    editedMap->doors[dId].selected = true;
    selectedDoor = dId;
    editMode = EDIT_MODE_DOORS;
}

bool Editor::doorAdd(int node, uint16_t texId, int & dId)
{
    Door d{};
    d.nodeID = node;
    d.width = 8.0f;
    d.height = 16.0f;
    d.thick = 0.25f;
    d.angle = 0.0f;
    d.swing = 90.0f;
    d.time = 2.0f;
    d.mode = DOOR_MODE_PIVOT;
    d.easing = EASING_IN_OUT_QUAD;

    d.surfaces[DOOR_SURFACE_FRONT].id = texId;
    d.surfaces[DOOR_SURFACE_FRONT].scaleX = 4.0f;
    d.surfaces[DOOR_SURFACE_FRONT].scaleY = 4.0f;

    d.surfaces[DOOR_SURFACE_BACK].id = texId;
    d.surfaces[DOOR_SURFACE_BACK].scaleX = 4.0f;
    d.surfaces[DOOR_SURFACE_BACK].scaleY = 4.0f;

    d.surfaces[DOOR_SURFACE_SIDE].id = 0;
    d.surfaces[DOOR_SURFACE_SIDE].scaleX = 4.0f;
    d.surfaces[DOOR_SURFACE_SIDE].scaleY = 4.0f;

    editedMap->doors.append(d);
    dId = editedMap->doors.count() - 1;
    return true;
}

void Editor::doorDelete(int dId)
{
    if (dId < 0 || dId >= editedMap->doors.count()) return;
    editedMap->doors.removeAt(dId);
}

void Editor::doorSelectAll()
{
    for (int i = 0; i < editedMap->doors.count(); i++)
        editedMap->doors[i].selected = true;
}

void Editor::doorDeselectAll()
{
    for (int i = 0; i < editedMap->doors.count(); i++)
        editedMap->doors[i].selected = false;
}

bool Editor::doorFindInCircle(VIEW_MODES mode, QVector2D pos, int & dId)
{
    return findInCircle<Door>(mode, pos, dId,
        editedMap->doors,
        [](const Door& d) { return d.nodeID; },
        selectedDoor);
}

bool Editor::doorFindInRect(VIEW_MODES mode, QVector2D c1, QVector2D c2, int & dId)
{
    return findInRect<Door>(mode, c1, c2, dId,
        editedMap->doors,
        [](const Door& d) { return d.nodeID; });
}

/*****************************************************************************/
void Editor::liftSelect(int eId)
{
    if (eId >= editedMap->lifts.count()) return;
    editedMap->lifts[eId].selected = true;
    selectedLift = eId;
    editMode = EDIT_MODE_LIFTS;
}

bool Editor::liftAdd(int node, uint16_t texId, int & eId)
{
    Lift e{};
    e.nodeID = node;
    e.width = 8.0f;
    e.length = 8.0f;
    e.thick = 1.0f;
    e.travel = 16.0f;
    e.time = 2.0f;
    e.mode = LIFT_MODE_Y_AXIS;
    e.easing = EASING_IN_OUT_QUAD;

    e.surfaces[LIFT_SURFACE_TOP].id = texId;
    e.surfaces[LIFT_SURFACE_TOP].scaleX = 4.0f;
    e.surfaces[LIFT_SURFACE_TOP].scaleY = 4.0f;

    e.surfaces[LIFT_SURFACE_BOTTOM].id = texId;
    e.surfaces[LIFT_SURFACE_BOTTOM].scaleX = 4.0f;
    e.surfaces[LIFT_SURFACE_BOTTOM].scaleY = 4.0f;

    e.surfaces[LIFT_SURFACE_SIDES].id = 0;
    e.surfaces[LIFT_SURFACE_SIDES].scaleX = 4.0f;
    e.surfaces[LIFT_SURFACE_SIDES].scaleY = 4.0f;

    editedMap->lifts.append(e);
    eId = editedMap->lifts.count() - 1;
    return true;
}

void Editor::liftDelete(int eId)
{
    if (eId < 0 || eId >= editedMap->lifts.count()) return;
    editedMap->lifts.removeAt(eId);
}

void Editor::liftSelectAll()
{
    for (int i = 0; i < editedMap->lifts.count(); i++)
        editedMap->lifts[i].selected = true;
}

void Editor::liftDeselectAll()
{
    for (int i = 0; i < editedMap->lifts.count(); i++)
        editedMap->lifts[i].selected = false;
}

bool Editor::liftFindInCircle(VIEW_MODES mode, QVector2D pos, int & eId)
{
    return findInCircle<Lift>(mode, pos, eId,
        editedMap->lifts,
        [](const Lift& e) { return e.nodeID; },
        selectedLift);
}

bool Editor::liftFindInRect(VIEW_MODES mode, QVector2D c1, QVector2D c2, int & eId)
{
    return findInRect<Lift>(mode, c1, c2, eId,
        editedMap->lifts,
        [](const Lift& e) { return e.nodeID; });
}

/*****************************************************************************/
void Editor::spriteSelect(int bId)
{
    if (bId >= editedMap->sprites.count()) return;
    editedMap->sprites[bId].selected = true;
    selectedSprite = bId;
    editMode = EDIT_MODE_SPRITES;
}

bool Editor::spriteAdd(int node, uint16_t texId, int & bId)
{
    Sprite b{};
    b.nodeID = node;

    b.width = 16.0f;
    b.height = 16.0f;
    b.pan = 0.0f;
    b.surface.id = texId;
    b.surface.scaleX = 4.0f;
    b.surface.scaleY = 4.0f;
    b.flags = SPRITE_FLAG_AUTOPAN;

    editedMap->sprites.append(b);
    bId = editedMap->sprites.count() - 1;
    return true;
}

void Editor::spriteDelete(int bId)
{
    if (bId < 0 || bId >= editedMap->sprites.count()) return;
    editedMap->sprites.removeAt(bId);
}

void Editor::spriteSelectAll()
{
    for (int i = 0; i < editedMap->sprites.count(); i++)
        editedMap->sprites[i].selected = true;
}

void Editor::spriteDeselectAll()
{
    for (int i = 0; i < editedMap->sprites.count(); i++)
        editedMap->sprites[i].selected = false;
}

bool Editor::spriteFindInCircle(VIEW_MODES mode, QVector2D pos, int & bId)
{
    return findInCircle<Sprite>(mode, pos, bId,
        editedMap->sprites,
        [](const Sprite& s) { return s.nodeID; },
        selectedSprite);
}

bool Editor::spriteFindInRect(VIEW_MODES mode, QVector2D c1, QVector2D c2, int & bId)
{
    return findInRect<Sprite>(mode, c1, c2, bId,
        editedMap->sprites,
        [](const Sprite& s) { return s.nodeID; });
}

/*****************************************************************************/
void Editor::staircaseSelect(int hId)
{
    if (hId >= editedMap->staircases.count()) return;
    editedMap->staircases[hId].selected = true;
    selectedStaircase = hId;
    editMode = EDIT_MODE_STAIRCASES;
}

bool Editor::staircaseAdd(int node, uint16_t texId, int & hId)
{
    Staircase h{};
    h.nodeID = node;
    h.pan = 0.0f;
    h.height = 16.0f;
    h.width = 8.0f;
    h.length = 16.0f;
    h.steps = 16;

    h.surfaces[STAIRCASE_SURFACE_STEPFALL].id = texId;
    h.surfaces[STAIRCASE_SURFACE_STEPFALL].scaleX = 4.0f;
    h.surfaces[STAIRCASE_SURFACE_STEPFALL].scaleY = 4.0f;

    h.surfaces[STAIRCASE_SURFACE_STEPTOP].id = texId;
    h.surfaces[STAIRCASE_SURFACE_STEPTOP].scaleX = 4.0f;
    h.surfaces[STAIRCASE_SURFACE_STEPTOP].scaleY = 4.0f;

    h.surfaces[STAIRCASE_SURFACE_SIDES].id = texId;
    h.surfaces[STAIRCASE_SURFACE_SIDES].scaleX = 4.0f;
    h.surfaces[STAIRCASE_SURFACE_SIDES].scaleY = 4.0f;

    editedMap->staircases.append(h);
    hId = editedMap->staircases.count() - 1;
    return true;
}

void Editor::staircaseDelete(int hId)
{
    if (hId < 0 || hId >= editedMap->staircases.count()) return;
    editedMap->staircases.removeAt(hId);
}

void Editor::staircaseSelectAll()
{
    for (int i = 0; i < editedMap->staircases.count(); i++)
        editedMap->staircases[i].selected = true;
}

void Editor::staircaseDeselectAll()
{
    for (int i = 0; i < editedMap->staircases.count(); i++)
        editedMap->staircases[i].selected = false;
}

bool Editor::staircaseFindInCircle(VIEW_MODES mode, QVector2D pos, int & hId)
{
    return findInCircle<Staircase>(mode, pos, hId,
        editedMap->staircases,
        [](const Staircase& s) { return s.nodeID; },
        selectedStaircase);
}

bool Editor::staircaseFindInRect(VIEW_MODES mode, QVector2D c1, QVector2D c2, int & hId)
{
    return findInRect<Staircase>(mode, c1, c2, hId,
        editedMap->staircases,
        [](const Staircase& s) { return s.nodeID; });
}

/*****************************************************************************/
void Editor::lightSelect(int lId)
{
    if (lId >= editedMap->lights.count()) return;
    editedMap->lights[lId].selected = true;
    selectedLight = lId;
    editMode = EDIT_MODE_LIGHTS;
}

bool Editor::lightAdd(int node, int & lId)
{
    Light l{};
    l.nodeID = node;
    l.colorA = 0xFFFFFF;
    l.colorB = 0x404040;
    l.strength = 4.0f;
    l.speed = 1.0f;
    l.anim = LIGHT_ANIM_COLOR_A;
    l.flags = LIGHT_FLAG_ENABLE;

    editedMap->lights.append(l);
    lId = editedMap->lights.count() - 1;
    return true;
}

void Editor::lightDelete(int lId)
{
    if (lId < 0 || lId >= editedMap->lights.count()) return;
    editedMap->lights.removeAt(lId);
}

void Editor::lightSelectAll()
{
    for (int i = 0; i < editedMap->lights.count(); i++)
        editedMap->lights[i].selected = true;
}

void Editor::lightDeselectAll()
{
    for (int i = 0; i < editedMap->lights.count(); i++)
        editedMap->lights[i].selected = false;
}

bool Editor::lightFindInCircle(VIEW_MODES mode, QVector2D pos, int & lId)
{
    return findInCircle<Light>(mode, pos, lId,
        editedMap->lights,
        [](const Light& s) { return s.nodeID; },
        selectedLight);
}

bool Editor::lightFindInRect(VIEW_MODES mode, QVector2D c1, QVector2D c2, int & lId)
{
    return findInRect<Light>(mode, c1, c2, lId,
        editedMap->lights,
        [](const Light& s) { return s.nodeID; });
}

/*****************************************************************************/
void Editor::speakerSelect(int aId)
{
    if (aId >= editedMap->speakers.count()) return;
    editedMap->speakers[aId].selected = true;
    selectedSpeaker = aId;
    editMode = EDIT_MODE_SPEAKERS;
}

bool Editor::speakerAdd(int node, int & aId)
{
    Speaker a{};
    a.nodeID = node;

    a.volume = 1.0f;
    a.size = 2.0f;
    a.pan = 0.0f;
    a.flags = SPEAKER_FLAG_OMNI | SPEAKER_FLAG_TRIGGER;

    editedMap->speakers.append(a);
    aId = editedMap->speakers.count() - 1;
    return true;
}

void Editor::speakerDelete(int aId)
{
    if (aId < 0 || aId >= editedMap->speakers.count()) return;
    editedMap->speakers.removeAt(aId);

// Keep the sounds list aligned with the speakers
    if (aId < editedMap->sounds.count()) {
        audio.soundUnload(editedMap->sounds[aId]);
        editedMap->sounds.removeAt(aId);
    }
}

void Editor::speakerSelectAll()
{
    for (int i = 0; i < editedMap->speakers.count(); i++)
        editedMap->speakers[i].selected = true;
}

void Editor::speakerDeselectAll()
{
    for (int i = 0; i < editedMap->speakers.count(); i++)
        editedMap->speakers[i].selected = false;
}

bool Editor::speakerFindInCircle(VIEW_MODES mode, QVector2D pos, int & aId)
{
    return findInCircle<Speaker>(mode, pos, aId,
        editedMap->speakers,
        [](const Speaker& s) { return s.nodeID; },
        selectedSpeaker);
}

bool Editor::speakerFindInRect(VIEW_MODES mode, QVector2D c1, QVector2D c2, int & aId)
{
    return findInRect<Speaker>(mode, c1, c2, aId,
        editedMap->speakers,
        [](const Speaker& s) { return s.nodeID; });
}

/*****************************************************************************/
void Editor::pathSelect(int pId)
{
    if (pId >= editedMap->paths.count()) return;
    editedMap->paths[pId].selected = true;
    selectedPath = pId;
    editMode = EDIT_MODE_PATHS;
}

bool Editor::pathAdd(int & pId)
{
    Path p{};
    strcpy(p.name, "NewPath");

    editedMap->paths.append(p);
    pId = editedMap->paths.count() - 1;
    return true;
}

void Editor::pathDelete(int pId)
{
    if (pId < 0 || pId >= editedMap->paths.count()) return;
    editedMap->paths.removeAt(pId);
}

void Editor::pathSelectAll()
{
    for (int i = 0; i < editedMap->paths.count(); i++)
        editedMap->paths[i].selected = true;
}

void Editor::pathDeselectAll()
{
    for (int i = 0; i < editedMap->paths.count(); i++)
        editedMap->paths[i].selected = false;
}

/*****************************************************************************/
float Editor::effectiveGridSize() const
{
    if (!gridSnap || !gridSize) return 0.25f;
    return gridSize;
}

/*****************************************************************************/
void Editor::snapCoords(QVector2D & pos)
{
    float gs = effectiveGridSize();
    float r = 0.5f * gs;
    float rx = copysignf(r, pos.x());
    float rz = copysignf(r, pos.y());
    pos.setX((int)((pos.x() + rx) / gs) * gs);
    pos.setY((int)((pos.y() + rz) / gs) * gs);
}

/*****************************************************************************/
QVector2D Editor::to2D(VIEW_MODES mode, const QVector3D & pos)
{
    if (mode == VIEW_TOP) return QVector2D(pos.x(), pos.z());
    if (mode == VIEW_SIDE) return QVector2D(-pos.z(), -pos.y());
    if (mode == VIEW_FRONT) return QVector2D(pos.x(), -pos.y());
    return QVector2D(0.0f, 0.0f);
}

QVector3D Editor::to3D(VIEW_MODES mode, const QVector2D & pos, const QVector3D & ref)
{
    if (mode == VIEW_TOP) return QVector3D(pos.x(), ref.y(), pos.y());
    if (mode == VIEW_SIDE) return QVector3D(ref.x(), -pos.y(), -pos.x());
    if (mode == VIEW_FRONT) return QVector3D(pos.x(), -pos.y(), ref.z());
    return ref;
}

/*****************************************************************************/
template <typename T, typename Container>
bool Editor::findInCircle(
    VIEW_MODES mode,
    const QVector2D& pos,
    int& outId,
    const Container& items,
    std::function<int(const T&)> getNodeId,
    int selectedIndex)
{
    outId = -1;
    float rMin = EDITOR_SPRITE_RADIUS / zoom;
    rMin = std::max(rMin, 0.5f * effectiveGridSize());

    int count = items.count();
    int offset = selectedIndex < 0 ? 0 : selectedIndex + 1;

    for (int i = 0; i < count; i++) {
        int index = (i + offset) % count;
        const T & item = items[index];
        const Node & n = editedMap->nodes[getNodeId(item)];
        if (!inView(n.pos)) continue;
        QVector2D np = to2D(mode, n.pos);
        float r = np.distanceToPoint(pos);
        if (r > rMin) continue;
        rMin = r;
        outId = index;
        break;
    }

    return outId != -1;
}

template <typename T, typename Container>
bool Editor::findInRect(
    VIEW_MODES mode,
    const QVector2D & c1,
    const QVector2D & c2,
    int& outId,
    const Container & items,
    std::function<int(const T &)> getNodeId)
{
    outId = -1;

    for (int i = 0; i < items.count(); i++) {
        const T & item = items[i];
        Node & n = editedMap->nodes[getNodeId(item)];
        if (!inView(n.pos)) continue;
        QVector2D np = to2D(mode, n.pos);
        QVector2D d1 = c1 - np;
        QVector2D d2 = c2 - np;
        if (d1.x() * d2.x() > 0.0f) continue;
        if (d1.y() * d2.y() > 0.0f) continue;
        n.selected = true;
        outId = i;
    }

    return outId != -1;
}
