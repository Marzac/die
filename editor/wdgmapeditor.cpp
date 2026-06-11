/**
    WALLER
    Map editor for the DIE engine
    (c) Fred's Lab 2024-2026
    Frédéric Meslin / info@fredslab.net
    SPDX-License-Identifier: MIT
    If used commercially, contributions, donations are highly appreciated.

    2D map editor widget
*/

#include "wdgmapeditor.h"

#include "globals.h"
#include "editor.h"
#include "mainwindow.h"

#include <QPainter>
#include <QMouseEvent>

#include <math.h>

/*****************************************************************************/
QBrush WdgMapEditor::gridMajor = QColor(96, 96, 96);
QBrush WdgMapEditor::gridMinor = QColor(32, 32, 32);
QBrush WdgMapEditor::gridScissor = QColor(128, 128, 128);

QPen WdgMapEditor::colorPrincipal = QColor(255, 255, 255);
QPen WdgMapEditor::colorSelected = QColor(192, 192, 192);
QPen WdgMapEditor::colorGreyed = QColor(32, 32, 32);

QPen WdgMapEditor::colorNodeBase = QColor(192, 32, 32);
QPen WdgMapEditor::colorWallBase = QColor(32, 32, 255);
QPen WdgMapEditor::colorWallNormals = QColor(0, 192, 0);
QPen WdgMapEditor::colorSubmapBase = QColor(255, 0, 255);
QPen WdgMapEditor::colorDoorBase = QColor(192, 0, 255);
QPen WdgMapEditor::colorLiftBase = QColor(255, 160, 0);
QPen WdgMapEditor::colorStaircaseBase = QColor(192, 32, 32);
QPen WdgMapEditor::colorLightBase = QColor(32, 32, 192);
QPen WdgMapEditor::colorSpriteBase = QColor(32, 192, 32);
QPen WdgMapEditor::colorSpeakerBase = QColor(0, 192, 192);

QPen WdgMapEditor::colorGlowMap = QColor(64, 0, 0);

QBrush WdgMapEditor::brushWallAnchor = QColor(32, 32, 255);

QImage WdgMapEditor::pictoSubmap;
QImage WdgMapEditor::pictoDoor;
QImage WdgMapEditor::pictoLift;
QImage WdgMapEditor::pictoStaircase;
QImage WdgMapEditor::pictoLight;
QImage WdgMapEditor::pictoSprite;
QImage WdgMapEditor::pictoSpeaker;

/*****************************************************************************/
WdgMapEditor::WdgMapEditor(QWidget *parent) :
    QWidget(parent),
    mode(VIEW_TOP),
    offset(), scroll(),
    selectRegion(false),
    selectRegionStart(false),
    selectRegionC1(), selectRegionC2(),
    mouseLeftWasPressed(false)
{
    if (pictoSubmap.isNull()) pictoSubmap = QImage(":/Icons/tools-submap.png");
    if (pictoDoor.isNull()) pictoDoor = QImage(":/Icons/tools-door.png");
    if (pictoLift.isNull()) pictoLift = QImage(":/Icons/tools-lift.png");
    if (pictoStaircase.isNull()) pictoStaircase = QImage(":/Icons/tools-staircase.png");
    if (pictoLight.isNull()) pictoLight = QImage(":/Icons/tools-light.png");
    if (pictoSprite.isNull()) pictoSprite = QImage(":/Icons/tools-sprite.png");
    if (pictoSpeaker.isNull()) pictoSpeaker = QImage(":/Icons/tools-speaker.png");

    setMouseTracking(true);
}

/*****************************************************************************/
void WdgMapEditor::setViewMode(VIEW_MODES mode)
{
    this->mode = mode;
}

/*****************************************************************************/
void WdgMapEditor::setCenter()
{
    offset = QVector2D(0.0f, 0.0f);
    update();
}

void WdgMapEditor::setOffsetX(float x)
{
    if (mode == VIEW_TOP || mode == VIEW_FRONT) {
        offset.setX(-x);
        update();
    }
}

void WdgMapEditor::setOffsetZ(float z)
{
    if (mode == VIEW_TOP || mode == VIEW_SIDE) {
        offset.setY(-z);
        update();
    }
}

/*****************************************************************************/
void WdgMapEditor::drawViewLabel(QPainter & painter)
{
    static const char * viewLabels[] = {
        "Top",      // VIEW_TOP
        "Side",     // VIEW_SIDE
        "Front",    // VIEW_FRONT
        "3D",       // VIEW_3D
    };

    QFont font = painter.font();
    font.setBold(true);
    font.setPointSize(12);
    painter.setFont(font);
    painter.setPen(Qt::white);

    QRect textRect(10, 10, width() - 20, 30);
    painter.drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter, viewLabels[mode]);
}

void WdgMapEditor::drawGrid(QPainter & painter, QVector2D & org)
{
    int w = width();
    int h = height();

    float grid = editor.gridSize * editor.zoom;
    painter.fillRect(0, 0, w, h, QBrush(Qt::black));

 // Map grid
    int gx = (int) (1.0f + w / grid);
    float sx = fmodf(org.x(), grid);
    for (int i = 0; i < gx; i++)
        painter.fillRect(sx + i * grid, 0, 1, h, gridMinor);
    painter.fillRect(org.x(), 0, 1, h, gridMajor);

    int gy = (int) (1.0f + h / grid);
    float sy = fmodf(org.y(), grid);
    for (int i = 0; i < gy; i++)
        painter.fillRect(0, sy + i * grid, w, 1, gridMinor);
    painter.fillRect(0, org.y(), w, 1, gridMajor);
}

void WdgMapEditor::drawGlowMap(QPainter & painter, QVector2D & org)
{
    if (mode != VIEW_TOP) return;

    float area = renderer.getGlowmapArea() * editor.zoom;

    QVector2D av = QVector2D(area, area);
    QVector2D ao = org - 0.5f * av;

    painter.setPen(colorGlowMap);
    painter.drawRect(ao.x(), ao.y(), av.x(), av.y());
}

void WdgMapEditor::drawArrow(QPainter & painter, QVector2D & org, QVector2D & pos, float pan, float length)
{
    painter.setRenderHint(QPainter::Antialiasing, true);
    QPointF o = (org + pos * editor.zoom).toPointF();
    QPointF f = QPointF(cosf(pan), sinf(pan)) * length;
    QPointF p = QPointF(f.y(), -f.x()) * 0.25f;

    QPointF triangle[3] = {o + p, o + f, o - p};
    painter.setPen(Qt::green);
    painter.drawPolygon(triangle, 3);
    painter.setRenderHint(QPainter::Antialiasing, false);
}

/*****************************************************************************/
void WdgMapEditor::drawMap(QPainter & painter, Map & map)
{
    QVector2D org = QVector2D(width() * 0.5f, height() * 0.5f) + offset;

    drawGrid(painter, org);
    drawGlowMap(painter, org);
    drawNodes(painter, map, org);
    if (mode == VIEW_TOP) drawWallsTop(painter, map, org);
    else drawWallsSide(painter, map, org);

    drawSubmaps(painter, map, org);
    drawDoors(painter, map, org);
    drawLifts(painter, map, org);
    drawStaircases(painter, map, org);
    drawLights(painter, map, org);
    drawSprites(painter, map, org);
    drawSpeakers(painter, map, org);
}

/*****************************************************************************/
void WdgMapEditor::drawNodes(QPainter & painter, Map & map, QVector2D & org)
{
    painter.setRenderHint(QPainter::Antialiasing, true);

    for (int pass = 0; pass < 3; pass++) {
        for (int i = 0; i < map.nodes.count(); i++) {
            Node & n = map.nodes[i];

            if (pass == 0) {if (n.selected || i == editor.selectedNode) continue;}
            else if (pass == 1) {if (!n.selected) continue;}
            else if (i != editor.selectedNode) continue;

            QVector2D np = Editor::to2D(mode, n.pos);
            np = org + np * editor.zoom;

            if (i == editor.selectedNode) painter.setPen(colorPrincipal);
            else if (n.selected) painter.setPen(colorSelected);
            else if (editor.inView(n.pos)) painter.setPen(colorNodeBase);
            else painter.setPen(colorGreyed);

            painter.drawEllipse(np.toPointF(), 3.0f, 3.0f);
            painter.setPen(colorNodeBase);
        }
    }
    painter.setRenderHint(QPainter::Antialiasing, false);
}

void WdgMapEditor::drawWallsTop(QPainter & painter, Map & map, QVector2D & org)
{
// Walls borders
    painter.setRenderHint(QPainter::Antialiasing, true);
    for (int pass = 0; pass < 3; pass++) {
        for (int i = 0; i < map.walls.count(); i++) {
            Wall & w = map.walls[i];

            if (pass == 0) {if (w.selected || i == editor.selectedWall) continue;}
            else if (pass == 1) {if (!w.selected) continue;}
            else if (i != editor.selectedWall) continue;

            Node & n1 = map.nodes[w.nodeID1];
            Node & n2 = map.nodes[w.nodeID2];
            QVector2D n1p = Editor::to2D(mode, n1.pos);
            QVector2D n2p = Editor::to2D(mode, n2.pos);
            n1p = org + n1p * editor.zoom;
            n2p = org + n2p * editor.zoom;

            if (i == editor.selectedWall) painter.setPen(colorPrincipal);
            else if (w.selected) painter.setPen(colorSelected);
            else if (editor.inView(n1.pos)) painter.setPen(colorWallBase);
            else painter.setPen(colorGreyed);

            painter.drawLine(n1p.toPointF(), n2p.toPointF());
        }
    }

// Walls normals
    painter.setPen(colorWallNormals);
    for(int i = 0; i < map.walls.count(); i++) {
        Wall & w = map.walls[i];
        Node & n1 = map.nodes[w.nodeID1];
        Node & n2 = map.nodes[w.nodeID2];
        if (!editor.inView(n1.pos)) continue;

        QVector2D n1p = Editor::to2D(mode, n1.pos);
        QVector2D n2p = Editor::to2D(mode, n2.pos);

        QVector2D nm = n2p - n1p;
        nm = QVector2D(nm.y(), -nm.x()).normalized();
        n1p = org + (n1p + n2p) * 0.5f * editor.zoom;
        n2p = n1p + nm * 8.0f;

        if (!(w.flags & WALL_FLAG_BACKCULLED))
            n1p -= nm * 4.0f;
        painter.drawLine(n1p.toPointF(), n2p.toPointF());
    }

    painter.setRenderHint(QPainter::Antialiasing, false);
}

void WdgMapEditor::drawWallsSide(QPainter & painter, Map & map, QVector2D & org)
{
// Minimum & maximum height
    float minY = org.y() - editor.viewMinY * editor.zoom;
    float maxY = org.y() - editor.viewMaxY * editor.zoom;
    painter.fillRect(0, minY, width(), 1, gridScissor);
    painter.fillRect(0, maxY, width(), 1, gridScissor);

// Walls borders
    painter.setRenderHint(QPainter::Antialiasing, true);
    for (int pass = 0; pass < 3; pass++) {
        for(int i = 0; i < map.walls.count(); i++) {
            Wall & w = map.walls[i];

            if (pass == 0) {if (w.selected || i == editor.selectedWall) continue;}
            else if (pass == 1) {if (!w.selected) continue;}
            else if (i != editor.selectedWall) continue;

            Node & n1 = map.nodes[w.nodeID1];
            Node & n2 = map.nodes[w.nodeID2];

            float wBot = n1.pos.y();
            float wTop = wBot + w.height;

            QVector2D n1p = Editor::to2D(mode, n1.pos);
            QVector2D n2p = Editor::to2D(mode, n2.pos);

            n1p.setY(-wTop);
            n2p.setY(-wBot);
            n1p = org + n1p * editor.zoom;
            n2p = org + n2p * editor.zoom;
            QVector2D dp = n2p - n1p;

            if (i == editor.selectedWall) painter.setPen(colorPrincipal);
            else if (w.selected) painter.setPen(colorSelected);
            else if (editor.inView(n1.pos)) painter.setPen(colorWallBase);
            else painter.setPen(colorGreyed);

            painter.drawRect(n1p.x(), n1p.y(), dp.x(), dp.y());
            painter.setPen(Qt::NoPen);

            if (!editor.inView(n1.pos)) continue;
            QVector2D mp = (n1p + n2p) * 0.5f - QVector2D(2.0f, 2.0f);
            painter.setBrush(brushWallAnchor);
            painter.drawRect(mp.x(), mp.y(), 4.0f, 4.0f);
            painter.setBrush(Qt::NoBrush);
        }
    }

    painter.setRenderHint(QPainter::Antialiasing, false);
}

/*****************************************************************************/
void WdgMapEditor::drawSubmaps(QPainter & painter, Map & map, QVector2D & org)
{
    drawItems<Submap>(painter, map, org,
        map.submaps,
        editor.selectedSubmap,
        colorSubmapBase,
        pictoSubmap,
        [](const Submap& m){ return m.nodeID; });
}

void WdgMapEditor::drawDoors(QPainter & painter, Map & map, QVector2D & org)
{
    drawItems<Door>(painter, map, org,
        map.doors,
        editor.selectedDoor,
        colorDoorBase,
        pictoDoor,
        [](const Door& d){ return d.nodeID; });
}

void WdgMapEditor::drawLifts(QPainter & painter, Map & map, QVector2D & org)
{
    drawItems<Lift>(painter, map, org,
        map.lifts,
        editor.selectedLift,
        colorLiftBase,
        pictoLift,
        [](const Lift& e){ return e.nodeID; });
}

void WdgMapEditor::drawStaircases(QPainter & painter, Map & map, QVector2D & org)
{
    drawItems<Staircase>(painter, map, org,
        map.staircases,
        editor.selectedStaircase,
        colorStaircaseBase,
        pictoStaircase,
        [](const Staircase& h){ return h.nodeID; });
}

void WdgMapEditor::drawLights(QPainter & painter, Map & map, QVector2D & org)
{
    drawItems<Light>(painter, map, org,
        map.lights,
        editor.selectedLight,
        colorLightBase,
        pictoLight,
        [](const Light& h){ return h.nodeID; });
}

void WdgMapEditor::drawSprites(QPainter & painter, Map & map, QVector2D & org)
{
    drawItems<Sprite>(painter, map, org,
        map.sprites,
        editor.selectedSprite,
        colorSpriteBase,
        pictoSprite,
        [](const Sprite& s){ return s.nodeID; });
}

void WdgMapEditor::drawSpeakers(QPainter & painter, Map & map, QVector2D & org)
{
    drawItems<Speaker>(painter, map, org,
        map.speakers,
        editor.selectedSpeaker,
        colorSpeakerBase,
        pictoSpeaker,
        [](const Speaker& s){ return s.nodeID; });
}

/*****************************************************************************/
void WdgMapEditor::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, false);

// Map content
    if (!editor.editedMap) return;
    drawMap(painter, *editor.editedMap);

// Viewpoint
    Viewpoint & v = editor.viewPoint;
    QVector2D org = QVector2D(width() * 0.5f, height() * 0.5f) + offset;
    QVector2D pos = Editor::to2D(mode, v.pos);
    float angle = mode == VIEW_TOP ? v.pan : 0.0f;
    drawArrow(painter, org, pos, angle, 10.0f);

// Selection region
    if (selectRegion) {
        painter.setRenderHint(QPainter::Antialiasing, false);
        painter.setPen(Qt::white);
        painter.setBrush(QBrush(Qt::NoBrush));
        int rw = selectRegionC2.x() - selectRegionC1.x();
        int rh = selectRegionC2.y() - selectRegionC1.y();
        painter.drawRect(selectRegionC1.x(), selectRegionC1.y(), rw, rh);
    }

// View label
    drawViewLabel(painter);
}

/*****************************************************************************/
void WdgMapEditor::mousePressSelect(QMouseEvent *event)
{
    bool shift = event->modifiers() & Qt::ShiftModifier;
    QVector2D click = QVector2D(event->position());
    QVector2D pos = getWorldCoordinates(click);

    selectRegionStart = true;
    selectRegionC1 = selectRegionC2 = click;

    if (editor.editMode == EDIT_MODE_NODES ||
        editor.editMode == EDIT_MODE_PATHS) {
        int nId = 0;
        if (editor.nodeFindInCircle(mode, pos, nId)) {
            bool inRegion = editor.editedMap->nodes[nId].selected;
            if (!shift && !inRegion) editor.nodeDeselectAll();
            selectRegionStart = false;
            mainWindow->pushUndoState();
            editor.selectedNode = nId;
            editor.editedMap->nodes[nId].selected = true;

        }else{
            editor.selectedNode = EDIT_UNSELECTED;
            editor.nodeDeselectAll();
        }
        mainWindow->updateNodeProperties();

    }else if (editor.editMode == EDIT_MODE_WALLS) {
        int nIdHover = 0;
        bool nodeHover = editor.nodeFindInCircle(mode, pos, nIdHover);

        int wId = 0;
        if (!nodeHover && editor.wallFindInCircle(mode, pos, wId)) {
            bool inRegion = editor.editedMap->walls[wId].selected;
            if (!shift && !inRegion) editor.wallDeselectAll();
            selectRegionStart = false;
            mainWindow->pushUndoState();
            editor.selectedWall = wId;
            editor.editedMap->walls[wId].selected = true;

        }else{
            if (nodeHover) selectRegionStart = false;
            editor.selectedWall = EDIT_UNSELECTED;
            editor.wallDeselectAll();
        }
        mainWindow->updateWallProperties();

    }else if (editor.editMode == EDIT_MODE_SUBMAPS) {
        int mId = 0;
        if (editor.submapFindInCircle(mode, pos, mId)) {
            bool inRegion = editor.editedMap->submaps[mId].selected;
            if (!shift && !inRegion) editor.submapDeselectAll();
            selectRegionStart = false;
            mainWindow->pushUndoState();
            editor.selectedSubmap = mId;
            editor.editedMap->submaps[mId].selected = true;

        }else{
            if (editor.selectedSubmap != EDIT_UNSELECTED)
                mouseLeftWasPressed = false;
            editor.selectedSubmap = EDIT_UNSELECTED;
            editor.submapDeselectAll();
        }
        mainWindow->updateSubmapProperties();

    }else if (editor.editMode == EDIT_MODE_DOORS) {
        int dId = 0;
        if (editor.doorFindInCircle(mode, pos, dId)) {
            bool inRegion = editor.editedMap->doors[dId].selected;
            if (!shift && !inRegion) editor.doorDeselectAll();
            selectRegionStart = false;
            mainWindow->pushUndoState();
            editor.selectedDoor = dId;
            editor.editedMap->doors[dId].selected = true;

        }else{
            if (editor.selectedDoor != EDIT_UNSELECTED)
                mouseLeftWasPressed = false;
            editor.selectedDoor = EDIT_UNSELECTED;
            editor.doorDeselectAll();
        }
        mainWindow->updateDoorProperties();

    }else if (editor.editMode == EDIT_MODE_LIFTS) {
        int eId = 0;
        if (editor.liftFindInCircle(mode, pos, eId)) {
            bool inRegion = editor.editedMap->lifts[eId].selected;
            if (!shift && !inRegion) editor.liftDeselectAll();
            selectRegionStart = false;
            mainWindow->pushUndoState();
            editor.selectedLift = eId;
            editor.editedMap->lifts[eId].selected = true;

        }else{
            if (editor.selectedLift != EDIT_UNSELECTED)
                mouseLeftWasPressed = false;
            editor.selectedLift = EDIT_UNSELECTED;
            editor.liftDeselectAll();
        }
        mainWindow->updateLiftProperties();

    }else if (editor.editMode == EDIT_MODE_STAIRCASES) {
        int hId = 0;
        if (editor.staircaseFindInCircle(mode, pos, hId)) {
            bool inRegion = editor.editedMap->staircases[hId].selected;
            if (!shift && !inRegion) editor.staircaseDeselectAll();
            selectRegionStart = false;
            mainWindow->pushUndoState();
            editor.selectedStaircase = hId;
            editor.editedMap->staircases[hId].selected = true;

        }else{
            if (editor.selectedStaircase != EDIT_UNSELECTED)
                mouseLeftWasPressed = false;
            editor.selectedStaircase = EDIT_UNSELECTED;
            editor.staircaseDeselectAll();
        }
        mainWindow->updateStaircaseProperties();

    }else if (editor.editMode == EDIT_MODE_LIGHTS) {
        int lId = 0;
        if (editor.lightFindInCircle(mode, pos, lId)) {
            bool inRegion = editor.editedMap->lights[lId].selected;
            if (!shift && !inRegion) editor.lightDeselectAll();
            selectRegionStart = false;
            mainWindow->pushUndoState();
            editor.selectedLight = lId;
            editor.editedMap->lights[lId].selected = true;

        }else{
            if (editor.selectedLight != EDIT_UNSELECTED)
                mouseLeftWasPressed = false;
            editor.selectedLight = EDIT_UNSELECTED;
            editor.lightDeselectAll();
        }
        mainWindow->updateLightProperties();

    }else if (editor.editMode == EDIT_MODE_SPEAKERS) {
        int dId = 0;
        if (editor.speakerFindInCircle(mode, pos, dId)) {
            bool inRegion = editor.editedMap->speakers[dId].selected;
            if (!shift && !inRegion) editor.speakerDeselectAll();
            selectRegionStart = false;
            mainWindow->pushUndoState();
            editor.selectedSpeaker = dId;
            editor.editedMap->speakers[dId].selected = true;

        }else{
            if (editor.selectedSpeaker != EDIT_UNSELECTED)
                mouseLeftWasPressed = false;
            editor.selectedSpeaker = EDIT_UNSELECTED;
            editor.speakerDeselectAll();
        }
        mainWindow->updateSpeakerProperties();
    }
}

void WdgMapEditor::mousePressScroll(QMouseEvent *event)
{
    scroll = QVector2D(event->position());
}

/*****************************************************************************/
void WdgMapEditor::mouseMoveDrag(QMouseEvent *event)
{
    QVector2D click = QVector2D(event->position());
    editor.snapCoords(cursor);

    if (selectRegionStart) {
        selectRegion = true;
        selectRegionC2 = click;
        mouseLeftWasPressed = true;
        return;
    }

    if (editor.editMode == EDIT_MODE_NODES ||
        editor.editMode == EDIT_MODE_PATHS) {
        if (editor.selectedNode >= 0) {
            Node & n = editor.editedMap->nodes[editor.selectedNode];
            QVector3D dp = Editor::to3D(mode, cursor - Editor::to2D(mode, n.pos), QVector3D());
            n.pos = Editor::to3D(mode, cursor, n.pos);
            for (int i = 0; i < editor.editedMap->nodes.count(); i++) {
                if (i == editor.selectedNode) continue;
                Node & p = editor.editedMap->nodes[i];
                if (!p.selected) continue;
                p.pos += dp;
            }
            editor.editedMap->computeAllWalls();
            mainWindow->updateNodeProperties();
        }

    }else if (editor.editMode == EDIT_MODE_WALLS) {
        if (editor.selectedWall >= 0) {
            Wall & w = editor.editedMap->walls[editor.selectedWall];
            QVector3D dp = Editor::to3D(mode, cursor - dragCursor, QVector3D());

            QSet<int> movedNodes;
            movedNodes.insert(w.nodeID1);
            movedNodes.insert(w.nodeID2);
            for (int i = 0; i < editor.editedMap->walls.count(); i++) {
                Wall &ws = editor.editedMap->walls[i];
                if (!ws.selected) continue;
                movedNodes.insert(ws.nodeID1);
                movedNodes.insert(ws.nodeID2);
            }
            for (int nodeID : movedNodes)
                editor.editedMap->nodes[nodeID].pos += dp;

            editor.editedMap->computeAllWalls();
            mainWindow->updateWallProperties();
        }

    }else if (editor.editMode == EDIT_MODE_SUBMAPS) {
        if (dragNodeItems<Submap>(editor.editedMap->submaps, editor.selectedSubmap,
            [](const Submap & m){ return m.nodeID; }))
            mainWindow->updateSubmapProperties();

    }else if (editor.editMode == EDIT_MODE_DOORS) {
        if (dragNodeItems<Door>(editor.editedMap->doors, editor.selectedDoor,
            [](const Door & d){ return d.nodeID; }))
            mainWindow->updateDoorProperties();

    }else if (editor.editMode == EDIT_MODE_LIFTS) {
        if (dragNodeItems<Lift>(editor.editedMap->lifts, editor.selectedLift,
            [](const Lift & e){ return e.nodeID; }))
            mainWindow->updateLiftProperties();

    }else if (editor.editMode == EDIT_MODE_STAIRCASES) {
        if (dragNodeItems<Staircase>(editor.editedMap->staircases, editor.selectedStaircase,
            [](const Staircase & h){ return h.nodeID; }))
            mainWindow->updateStaircaseProperties();

    }else if (editor.editMode == EDIT_MODE_LIGHTS) {
        if (dragNodeItems<Light>(editor.editedMap->lights, editor.selectedLight,
            [](const Light & l){ return l.nodeID; })) {
            renderer.flags |= RENDERER_FLAG_GLOWMAP_REBUILD;
            mainWindow->updateLightProperties();
        }

    }else if (editor.editMode == EDIT_MODE_SPRITES) {
        if (dragNodeItems<Sprite>(editor.editedMap->sprites, editor.selectedSprite,
            [](const Sprite & b){ return b.nodeID; }))
            mainWindow->updateSpriteProperties();

    }else if (editor.editMode == EDIT_MODE_SPEAKERS) {
        if (dragNodeItems<Speaker>(editor.editedMap->speakers, editor.selectedSpeaker,
            [](const Speaker & a){ return a.nodeID; }))
            mainWindow->updateSpeakerProperties();
    }

    dragCursor = cursor;
}

/*****************************************************************************/
template <typename T, typename Container>
bool WdgMapEditor::dragNodeItems(Container & items, int selectedIndex,
                                 std::function<int(const T&)> getNodeId)
{
    if (selectedIndex < 0) return false;

// Drag the selected item's node to the cursor
    const T & item = items[selectedIndex];
    Node & n = editor.editedMap->nodes[getNodeId(item)];
    QVector3D dp = Editor::to3D(mode, cursor - Editor::to2D(mode, n.pos), QVector3D());
    n.pos = Editor::to3D(mode, cursor, n.pos);

// Move the other selected items' nodes by the same delta
    for (int i = 0; i < items.count(); i++) {
        if (i == selectedIndex) continue;
        Node & p = editor.editedMap->nodes[getNodeId(items[i])];
        if (!p.selected) continue;
        p.pos += dp;
    }
    return true;
}

void WdgMapEditor::mouseMoveScroll(QMouseEvent *event)
{
    offset += QVector2D(event->position()) - scroll;
    scroll = QVector2D(event->position());
}

/*****************************************************************************/
void WdgMapEditor::mouseReleaseCreate(QMouseEvent *)
{
    if (mode != VIEW_TOP) return;

    if (editor.editMode == EDIT_MODE_NODES ||
        editor.editMode == EDIT_MODE_PATHS) {
        bool selected = editor.selectedNode != EDIT_UNSELECTED;
        //editor.nodeDeselectAll();
        if (selected) return;

        int nId = 0;
        QVector3D mark(cursor.x(), 0.0f, cursor.y());
        mainWindow->pushUndoState();
        if (editor.nodeAdd(mark, nId)) {
            editor.selectedNode = nId;
            editor.editedMap->nodes[nId].selected = true;
            mainWindow->updateNodeProperties();
        }

    }else if (editor.editMode == EDIT_MODE_WALLS) {
        bool selected = editor.selectedWall != EDIT_UNSELECTED;
        //editor.wallDeselectAll();
        if (selected) return;

        int nId = 0;
        if (editor.nodeFindInCircle(mode, cursor, nId)) {
            if (editor.selectedNode != EDIT_UNSELECTED &&
                editor.selectedNode != nId) {
                int wId = 0;
                mainWindow->pushUndoState();
                editor.wallAdd(editor.selectedNode, nId, editor.selectedTextureID, wId);
                editor.editedMap->computeWall(wId);
                editor.selectedWall = wId;
                editor.editedMap->walls[wId].selected = true;

            }else editor.selectedWall = EDIT_UNSELECTED;

            editor.selectedNode = nId;
            editor.editedMap->nodes[nId].selected = true;
            mainWindow->updateNodeProperties();
            mainWindow->updateWallProperties();
        }
    }
}

void WdgMapEditor::mouseReleaseSelect(QMouseEvent *event)
{
    bool shift = event->modifiers() & Qt::ShiftModifier;
    if (!shift) {
        editor.nodeDeselectAll();
        editor.wallDeselectAll();
    }

    QVector2D c1 = getWorldCoordinates(selectRegionC1);
    QVector2D c2 = getWorldCoordinates(selectRegionC2);
    if (editor.editMode == EDIT_MODE_NODES ||
        editor.editMode == EDIT_MODE_PATHS) {
        int nId = 0;
        if (editor.nodeFindInRect(mode, c1, c2, nId)) {
            editor.selectedNode = nId;
            mainWindow->updateNodeProperties();
        }

    }else if (editor.editMode == EDIT_MODE_WALLS) {
        int wId = 0;
        if (editor.wallFindInRect(mode, c1, c2, wId)) {
            editor.selectedWall = wId;
            mainWindow->updateWallProperties();
        }

    }else if (editor.editMode == EDIT_MODE_SUBMAPS) {
        int mId = 0;
        if (editor.submapFindInRect(mode, c1, c2, mId)) {
            editor.selectedSubmap = mId;
            mainWindow->updateSubmapProperties();
        }

    }else if (editor.editMode == EDIT_MODE_DOORS) {
        int dId = 0;
        if (editor.doorFindInRect(mode, c1, c2, dId)) {
            editor.selectedDoor = dId;
            mainWindow->updateDoorProperties();
        }

    }else if (editor.editMode == EDIT_MODE_LIFTS) {
        int eId = 0;
        if (editor.liftFindInRect(mode, c1, c2, eId)) {
            editor.selectedLift = eId;
            mainWindow->updateLiftProperties();
        }

    }else if (editor.editMode == EDIT_MODE_STAIRCASES) {
        int hId = 0;
        if (editor.staircaseFindInRect(mode, c1, c2, hId)) {
            editor.selectedStaircase = hId;
            mainWindow->updateStaircaseProperties();
        }

    }else if (editor.editMode == EDIT_MODE_LIGHTS) {
        int lId = 0;
        if (editor.lightFindInRect(mode, c1, c2, lId)) {
            editor.selectedLight = lId;
            mainWindow->updateLightProperties();
        }

    }else if (editor.editMode == EDIT_MODE_SPRITES) {
        int bId = 0;
        if (editor.spriteFindInRect(mode, c1, c2, bId)) {
            editor.selectedSprite = bId;
            mainWindow->updateSpriteProperties();
        }

    }else if (editor.editMode == EDIT_MODE_SPEAKERS) {
        int aId = 0;
        if (editor.speakerFindInRect(mode, c1, c2, aId)) {
            editor.selectedSpeaker = aId;
            mainWindow->updateSpeakerProperties();
        }
    }
}

/*****************************************************************************/
void WdgMapEditor::mousePressEvent(QMouseEvent *event)
{
    cursor = getWorldCoordinates(QVector2D(event->position()));
    editor.snapCoords(cursor);
    dragCursor = cursor;

    mouseLeftWasPressed = false;
    if (event->buttons() & Qt::LeftButton) {
        mouseLeftWasPressed = true;
        mousePressSelect(event);

    }else if (event->buttons() & Qt::MiddleButton)
        mousePressScroll(event);
}

void WdgMapEditor::mouseReleaseEvent(QMouseEvent *event)
{
    if (mouseLeftWasPressed) {
        if (!selectRegion) mouseReleaseCreate(event);
        else mouseReleaseSelect(event);
        mainWindow->pushUndoState();
    }
    selectRegion = false;
    selectRegionStart = false;
    mouseLeftWasPressed = false;
}

void WdgMapEditor::mouseMoveEvent(QMouseEvent *event)
{
    cursor = getWorldCoordinates(QVector2D(event->position()));
    editor.snapCoords(cursor);

    if (event->buttons() & Qt::MiddleButton) mouseMoveScroll(event);
    if (event->buttons() & Qt::LeftButton) mouseMoveDrag(event);
}

/*****************************************************************************/
void WdgMapEditor::wheelEvent(QWheelEvent *event)
{
    constexpr float zoomMax = 128.0f;
    constexpr float zoomMin = 0.1073742f;   // 0.8^10, ten steps out

    float lastZoom = editor.zoom;

    QPoint degrees = event->angleDelta();
    if (degrees.y() > 0) editor.zoom *= 1.25f;
    if (degrees.y() < 0) editor.zoom *= 0.8f;
    if (editor.zoom > zoomMax) editor.zoom = zoomMax;
    if (editor.zoom < zoomMin) editor.zoom = zoomMin;

    QVector2D org = QVector2D(width() * 0.5f, height() * 0.5f) + offset;
    offset += (QVector2D(event->position()) - org) * (1.0f - editor.zoom / lastZoom);
    event->accept();

    update();
}

/*****************************************************************************/
QVector2D WdgMapEditor::getWorldCoordinates(const QVector2D & pos)
{
    QVector2D org = QVector2D(width() * 0.5f, height() * 0.5f) + offset;
    return (pos - org) / editor.zoom;
}

/*****************************************************************************/
template <typename T, typename Container>
void WdgMapEditor::drawItems(QPainter& painter,
                             Map& map,
                             QVector2D& org,
                             const Container& items,
                             int selectedIndex,
                             const QPen& pen,
                             const QImage& icon,
                             std::function<int(const T&)> getNodeId)
{
// Pass 1: non-selected
    for (int i = 0; i < items.count(); i++) {
        const T& item = items[i];
        if (item.selected || i == selectedIndex) continue;

        Node & n = map.nodes[getNodeId(item)];
        QVector2D np = Editor::to2D(mode, n.pos);
        np = org + np * editor.zoom;

        if (editor.inView(n.pos)) {
            painter.setPen(pen);
            painter.drawImage(np.toPointF() - QPointF(8.0f, 8.0f), icon);
        }else painter.setPen(colorGreyed);
        painter.drawRect(QRectF(np.x() - 10.0f, np.y() - 10.0f, 20.0f, 20.0f));
    }

// Pass 2: selected
    for (int i = 0; i < items.count(); i++) {
        const T& item = items[i];
        if (!item.selected || i == selectedIndex) continue;

        Node& n = map.nodes[getNodeId(item)];
        QVector2D np = Editor::to2D(mode, n.pos);
        np = org + np * editor.zoom;

        if (editor.inView(n.pos)) {
            painter.setPen(colorSelected);
            painter.drawImage(np.toPointF() - QPointF(8.0f, 8.0f), icon);
        }else painter.setPen(colorGreyed);
        painter.drawRect(QRectF(np.x() - 10.0f, np.y() - 10.0f, 20.0f, 20.0f));
    }

// Pass 3: current
    if (selectedIndex >= 0 && selectedIndex < items.count()) {
        const T& item = items[selectedIndex];
        Node& n = map.nodes[getNodeId(item)];
        QVector2D np = Editor::to2D(mode, n.pos);
        np = org + np * editor.zoom;

        if (editor.inView(n.pos)) {
            painter.setPen(colorPrincipal);
            painter.drawImage(np.toPointF() - QPointF(8.0f, 8.0f), icon);
        }else painter.setPen(colorGreyed);
        painter.drawRect(QRectF(np.x() - 10.0f, np.y() - 10.0f, 20.0f, 20.0f));
    }
}
