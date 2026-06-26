/**
    RIGGER
    Animation editor for the DIE engine
    (c) Fred's Lab 2024-2026
    Frédéric Meslin / info@fredslab.net
    SPDX-License-Identifier: MIT
    If used commercially, contributions, donations are highly appreciated.

    2D rig editor widget
*/

#ifndef WDG_RIGEDITOR_H
#define WDG_RIGEDITOR_H

#include <QWidget>
#include <QPen>
#include <QVector2D>

class WdgRigEditor : public QWidget
{
    Q_OBJECT

public:
    explicit WdgRigEditor(QWidget * parent = nullptr);

protected:
    void paintEvent(QPaintEvent * event) override;

    void mousePressEvent(QMouseEvent * event) override;
    void mouseMoveEvent(QMouseEvent * event) override;
    void mouseReleaseEvent(QMouseEvent * event) override;
    void wheelEvent(QWheelEvent * event) override;

private:
    QVector2D scroll;       ///< last cursor position during an orbit drag

    QVector2D pressWorld;   ///< world cursor at the left press (used to create)
    QVector2D selectRegionC1;
    QVector2D selectRegionC2;
    bool selectRegion;
    bool selectRegionStart;
    bool mouseLeftWasPressed;

    static QPen colorPrincipal;
    static QPen colorSelected;
    static QPen colorJointBase;
    static QPen colorBoneBase;
    static QPen colorAxis;

    QVector2D origin() const;
    QVector2D getWorldCoordinates(const QVector2D & screen) const;

    void drawAxes(QPainter & painter, const QVector2D & org);
    void drawBones(QPainter & painter, const QVector2D & org);
    void drawJoints(QPainter & painter, const QVector2D & org);
};

#endif // WDG_RIGEDITOR_H
