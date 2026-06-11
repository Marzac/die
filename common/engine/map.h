/**
    DIE ENGINE
    Depth Integration Engine / A modern ray-caster
    (c) Fred's Lab 2024-2026
    Frédéric Meslin / info@fredslab.net
    SPDX-License-Identifier: MIT
    If used commercially, contributions, donations are highly appreciated.

    map model
*/

#ifndef MAP_H
#define MAP_H

#include "mapobjects_eq.h"
#include "env.h"

#include <QList>
#include <QVector3D>
#include <QImage>

#include <stdio.h>
#include <stdint.h>

class QSpatialSound;

/*****************************************************************************/
/**
    \brief Snapshot of the persistent map data, used for the undo history
*/
struct MapState {
    QList<Node>       nodes;
    QList<Wall>       walls;
    QList<Submap>     submaps;
    QList<Door>       doors;
    QList<Lift>       lifts;
    QList<Sprite>     sprites;
    QList<Staircase>  staircases;
    QList<Light>      lights;
    QList<Speaker>    speakers;
    QList<Path>       paths;
    Sun sun;
    Fog fog;
};

bool operator==(const MapState & a, const MapState & b);

/*****************************************************************************/
class Map
{
public:
    Map();

    void init();
    void terminate();

    /// \brief Animate the doors and the lifts
    void update();

    /**
        \brief Save the map to a file
        \param filename path of the map file
        \param submap skip the environment section (submaps do not carry one)
        \return true on success, false when the file cannot be opened
    */
    bool save(const QString & filename, bool submap);

    /**
        \brief Load the map, then its submaps, sounds and textures
        \param filename path of the map file
        \return true on success
    */
    bool load(const QString & filename);

    /// \brief Push the map geometry and lights to the renderer
    void pass(const QVector3D & camPos);

    /// \brief Trigger the actionable objects (doors, lifts) close to a position
    void performAction(const QVector3D & pos);

    void doorOpen(uint16_t dId);
    void doorClose(uint16_t dId);
    void doorShake(uint16_t dId);

    void liftStart(uint16_t eId);
    void liftStop(uint16_t eId);

    void speakerApplyFlags(uint16_t sId);

    /// \brief Recompute the cached wall geometry (dir, normal, length)
    void computeAllWalls();
    void computeWall(uint16_t wId);

    QString getRelativePath(const QString & subMapPath);
    QString resolveRelativePath(const QString & subMapPath);

// Object lookups, by tag name and by object name
    QList<Node>    findNodesByTag(const char * tagName) const;
    QList<Submap>  findSubmapsByTag(const char * tagName) const;
    QList<Door>    findDoorsByTag(const char * tagName) const;
    QList<Lift>    findLiftsByTag(const char * tagName) const;
    QList<Sprite>  findSpritesByTag(const char * tagName) const;
    QList<Light>   findLightsByTag(const char * tagName) const;
    QList<Speaker> findSpeakersByTag(const char * tagName) const;
    QList<Path>    findPathsByTag(const char * tagName) const;

    int findSubmapByName(const char * name) const;
    int findDoorByName(const char * name) const;
    int findLiftByName(const char * name) const;
    int findSpriteByName(const char * name) const;
    int findSpeakerByName(const char * name) const;
    int findPathByName(const char * name) const;

    /// \brief Snapshot the persistent map data (see MapState)
    MapState captureState() const;

    /// \brief Restore a snapshot of the map data
    void restoreState(const MapState & state);

    QVector3D origin;
    float pan;
    float scale;

    QString path;
    QImage textures;
    Sun sun;
    Fog fog;
    QList<Node> nodes;
    QList<Wall> walls;
    QList<Submap> submaps;
    QList<Door> doors;
    QList<Lift> lifts;
    QList<Sprite> sprites;
    QList<Staircase> staircases;
    QList<Light> lights;
    QList<Speaker> speakers;
    QList<Path> paths;

    QList<Map> maps;
    QList<QSpatialSound *> sounds;

private:
    void clearObjects();
    void resetEnvironment();

    void passAsSub(const QVector3D & camPos);
    void passNodes();
    void passWalls(int nodeBase, int textureBase);
    void passDoors(int textureBase);
    void passLifts(int textureBase);
    void passSprites(int textureBase, const QVector3D & camPos);
    void passStaircases(int textureBase);
    void passLights(int nodeBase);

    void updateAllDoors();
    void updateAllLifts();

    bool loadSubmaps();
    bool loadSounds();

// Geometry & parsing utilities
    QVector3D getAbsCoords(const QVector3D & pos);
    static QVector3D rotateAroundY(const QVector3D & v, float angle);
    static QVector2D rotateAroundY(const QVector2D & v, float angle);
    static void skipLine(FILE * file);
};

#endif // MAP_H
