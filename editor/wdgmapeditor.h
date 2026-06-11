/**
    WALLER
    Map editor for the DIE engine
    (c) Fred's Lab 2024-2026
    Frédéric Meslin / info@fredslab.net
    SPDX-License-Identifier: MIT
    If used commercially, contributions, donations are highly appreciated.

    2D map editor widget
*/

#ifndef WDG_MAPEDITOR_H
#define WDG_MAPEDITOR_H

#include "editor.h"
#include "map.h"

#include <QWidget>
#include <QPen>
#include <QBrush>
#include <QVector2D>

#include <functional>

class WdgMapEditor : public QWidget
{
    Q_OBJECT

public:
    explicit WdgMapEditor(QWidget *parent = nullptr);

    void setViewMode(VIEW_MODES mode);

    void setCenter();
    void setOffsetX(float x);
    void setOffsetZ(float z);

    float getOffsetX() {return -offset.x();}
    float getOffsetZ() {return -offset.y();}

    QVector2D getCursor() {return cursor;}

private:
    VIEW_MODES mode;

    QVector2D offset;
    QVector2D scroll;
    QVector2D cursor;
    QVector2D dragCursor;

    bool selectRegion;
    bool selectRegionStart;
    QVector2D selectRegionC1;
    QVector2D selectRegionC2;
    bool mouseLeftWasPressed;

    static QBrush gridMajor;
    static QBrush gridMinor;
    static QBrush gridScissor;

    static QPen colorPrincipal;
    static QPen colorSelected;
    static QPen colorGreyed;

    static QPen colorNodeBase;
    static QPen colorWallBase;
    static QPen colorWallNormals;
    static QPen colorSubmapBase;
    static QPen colorDoorBase;
    static QPen colorLiftBase;
    static QPen colorStaircaseBase;
    static QPen colorLightBase;
    static QPen colorSpriteBase;
    static QPen colorSpeakerBase;

    static QPen colorGlowMap;

    static QBrush brushWallAnchor;

    static QImage pictoSubmap;
    static QImage pictoDoor;
    static QImage pictoLift;
    static QImage pictoStaircase;
    static QImage pictoLight;
    static QImage pictoSprite;
    static QImage pictoSpeaker;

protected:
    void paintEvent(QPaintEvent * event) override;

    void mouseMoveEvent(QMouseEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

    void wheelEvent(QWheelEvent *event) override;

private:
    void drawViewLabel(QPainter & painter);
    void drawGrid(QPainter & painter, QVector2D & org);
    void drawGlowMap(QPainter & painter, QVector2D & org);

    void drawArrow(QPainter & painter, QVector2D & org, QVector2D &pos, float pan, float length);
    void drawMap(QPainter & painter, Map & map);
    void drawNodes(QPainter & painter, Map & map, QVector2D & org);
    void drawWallsTop(QPainter & painter, Map & map, QVector2D & org);
    void drawWallsSide(QPainter & painter, Map & map, QVector2D & org);

    void drawSubmaps(QPainter & painter, Map & map, QVector2D & org);
    void drawDoors(QPainter & painter, Map & map, QVector2D & org);
    void drawLifts(QPainter & painter, Map & map, QVector2D & org);
    void drawStaircases(QPainter & painter, Map & map, QVector2D & org);
    void drawLights(QPainter & painter, Map & map, QVector2D & org);
    void drawSprites(QPainter & painter, Map & map, QVector2D & org);
    void drawSpeakers(QPainter & painter, Map & map, QVector2D & org);

    QVector2D getWorldCoordinates(const QVector2D & pos);
    void mousePressScroll(QMouseEvent *event);
    void mousePressSelect(QMouseEvent *event);
    void mouseReleaseCreate(QMouseEvent *event);
    void mouseReleaseSelect(QMouseEvent *event);

    void mouseMoveScroll(QMouseEvent *event);
    void mouseMoveDrag(QMouseEvent *event);

    template <typename T, typename Container>
    void drawItems(QPainter& painter,
        Map& map,
        QVector2D& org,
        const Container& items,
        int selectedIndex,
        const QPen& pen,
        const QImage& icon,
        std::function<int(const T&)> getNodeId);

    /// \brief Drag the selected item's node to the cursor, move the other selected items along
    template <typename T, typename Container>
    bool dragNodeItems(Container & items,
        int selectedIndex,
        std::function<int(const T&)> getNodeId);
};

#endif // WDG_MAPEDITOR_H
