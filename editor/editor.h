/**
    WALLER
    Map editor for the DIE engine
    (c) Fred's Lab 2024-2026
    Frédéric Meslin / info@fredslab.net
    SPDX-License-Identifier: MIT
    If used commercially, contributions, donations are highly appreciated.

    editor model
*/

#ifndef EDITOR_H
#define EDITOR_H

#include "env.h"
#include "map.h"
#include "renderer.h"

#include <QVector2D>
#include <QVector3D>
#include <QList>

#include <functional>
#include <stdint.h>

typedef enum {
    EDIT_MODE_NODES = 0,
    EDIT_MODE_WALLS,
    EDIT_MODE_SUBMAPS,
    EDIT_MODE_STAIRCASES,
    EDIT_MODE_DOORS,
    EDIT_MODE_LIFTS,
    EDIT_MODE_SPRITES,
    EDIT_MODE_LIGHTS,
    EDIT_MODE_SPEAKERS,
    EDIT_MODE_PATHS,
    EDIT_MODE_TAGS,
    EDIT_MODE_STAGE,
    EDIT_MODE_CONFIG,
} EDIT_MODES;

typedef enum {
    VIEW_TOP = 0,
    VIEW_SIDE,
    VIEW_FRONT,
    VIEW_3D,
} VIEW_MODES;

/*****************************************************************************/
constexpr int EDIT_UNSELECTED = -1;

constexpr int EDITOR_NODE_RADIUS = 8;
constexpr int EDITOR_WALL_RADIUS = 16;
constexpr int EDITOR_OBJECT_RADIUS = 12;

/*****************************************************************************/
class Editor
{
public:
    Editor();
    void init();
    void terminate();

    /// \brief Snap a 2D position to the active grid
    void snapCoords(QVector2D & pos);

    EDIT_MODES editMode;

    Viewpoint viewPoint;

    Env env;
    Map rootMap;
    Map * editedMap;

    int selectedNode;
    int selectedWall;
    int selectedSubmap;
    int selectedDoor;
    int selectedLift;
    int selectedSprite;
    int selectedStaircase;
    int selectedLight;
    int selectedSpeaker;
    int selectedPath;
    uint16_t selectedTextureID;

    QList<uint16_t> copyNodeIDs;
    QList<Node> copyNodes;
    QList<Wall> copyWalls;
    QList<Door>      copyDoors;
    QList<Lift>      copyLifts;
    QList<Sprite>    copySprites;
    QList<Staircase> copyStaircases;
    QList<Light>     copyLights;

    bool gridSnap;
    float gridSize;
    float zoom;
    float viewMinY;
    float viewMaxY;

    bool gravity;
    bool collisions;
    bool wallSelector;

    bool inView(const QVector3D & pos) const {
        return pos.y() >= viewMinY && pos.y() <= viewMaxY;
    }

    void selectAll();
    void deselect();
    void cut();
    void copy();

    /// \brief Paste the copied objects, positioned relative to the last copied node
    void paste(VIEW_MODES mode, QVector2D pos);
    void align();

// Per-object operations.
// The FindInCircle searches start right after the current selection,
// so repeated clicks cycle through overlapping objects.
    void nodeSelect(int nId);
    bool nodeAdd(QVector3D pos, int & nId);
    void nodeDelete(int nId);
    void nodeSelectAll();
    void nodeDeselectAll();
    bool nodeFindInCircle(VIEW_MODES mode, QVector2D pos, int & nId);
    bool nodeFindInRect(VIEW_MODES mode, QVector2D c1, QVector2D c2, int & nId);

    void wallSelect(int wId);
    bool wallAdd(int n1, int n2, uint16_t texId, int & wId);
    void wallDelete(int wId);
    void wallSelectAll();
    void wallDeselectAll();
    bool wallFindInCircle(VIEW_MODES mode, QVector2D pos, int & wId);
    bool wallFindInRect(VIEW_MODES mode, QVector2D c1, QVector2D c2, int & wId);

    void submapSelect(int mId);
    bool submapAdd(int node, int & mId);
    void submapDelete(int mId);
    void submapSelectAll();
    void submapDeselectAll();
    bool submapFindInCircle(VIEW_MODES mode, QVector2D pos, int & mId);
    bool submapFindInRect(VIEW_MODES mode, QVector2D c1, QVector2D c2, int & mId);

    void doorSelect(int dId);
    bool doorAdd(int node, uint16_t texId, int & dId);
    void doorDelete(int dId);
    void doorSelectAll();
    void doorDeselectAll();
    bool doorFindInCircle(VIEW_MODES mode, QVector2D pos, int & dId);
    bool doorFindInRect(VIEW_MODES mode, QVector2D c1, QVector2D c2, int & dId);

    void liftSelect(int eId);
    bool liftAdd(int node, uint16_t texId, int & eId);
    void liftDelete(int eId);
    void liftSelectAll();
    void liftDeselectAll();
    bool liftFindInCircle(VIEW_MODES mode, QVector2D pos, int & eId);
    bool liftFindInRect(VIEW_MODES mode, QVector2D c1, QVector2D c2, int & eId);

    void spriteSelect(int sId);
    bool spriteAdd(int node, uint16_t texId, int & sId);
    void spriteDelete(int sId);
    void spriteSelectAll();
    void spriteDeselectAll();
    bool spriteFindInCircle(VIEW_MODES mode, QVector2D pos, int & bId);
    bool spriteFindInRect(VIEW_MODES mode, QVector2D c1, QVector2D c2, int & bId);

    void staircaseSelect(int hId);
    bool staircaseAdd(int node, uint16_t texId, int & hId);
    void staircaseDelete(int hId);
    void staircaseSelectAll();
    void staircaseDeselectAll();
    bool staircaseFindInCircle(VIEW_MODES mode, QVector2D pos, int & hId);
    bool staircaseFindInRect(VIEW_MODES mode, QVector2D c1, QVector2D c2, int & hId);

    void lightSelect(int lId);
    bool lightAdd(int node, int & lId);
    void lightDelete(int lId);
    void lightSelectAll();
    void lightDeselectAll();
    bool lightFindInCircle(VIEW_MODES mode, QVector2D pos, int & lId);
    bool lightFindInRect(VIEW_MODES mode, QVector2D c1, QVector2D c2, int & lId);

    void speakerSelect(int aId);
    bool speakerAdd(int node, int & aId);
    void speakerDelete(int aId);
    void speakerSelectAll();
    void speakerDeselectAll();
    bool speakerFindInCircle(VIEW_MODES mode, QVector2D pos, int & aId);
    bool speakerFindInRect(VIEW_MODES mode, QVector2D c1, QVector2D c2, int & aId);

    void pathSelect(int pId);
    bool pathAdd(int & pId);
    void pathDelete(int pId);
    void pathSelectAll();
    void pathDeselectAll();

    /// \brief Project a world position onto a 2D editor view
    static QVector2D to2D(VIEW_MODES mode, const QVector3D & pos);

    /// \brief Lift a 2D editor view position back to world space (ref fills the missing axis)
    static QVector3D to3D(VIEW_MODES mode, const QVector2D & pos, const QVector3D & ref);

private:
    int findCopiedNodeID(uint16_t id);
    float effectiveGridSize() const;

// Copy / paste / delete of the node-bound objects (door, lift, sprite, ...).
// Each object carries a single nodeID, so copy also pulls in its node and
// paste remaps that nodeID onto the freshly pasted nodes.
    template <typename T>
    void copyObjects(const QList<T> & items, QList<T> & dst);

    template <typename T>
    void pasteObjects(const QList<T> & src, QList<T> & dst, int nodeBase, int & selected);

    template <typename T>
    void deleteSelected(QList<T> & items);

    template <typename T, typename Container>
    bool findInCircle(
        VIEW_MODES mode,
        const QVector2D & pos,
        int & outId,
        const Container & items,
        std::function<int(const T&)> getNodeId,
        int selectedIndex);

    template <typename T, typename Container>
    bool findInRect(
        VIEW_MODES mode,
        const QVector2D & c1,
        const QVector2D & c2,
        int & outId,
        const Container & items,
        std::function<int(const T&)> getNodeId);
};

extern Editor editor;

#endif // EDITOR_H
