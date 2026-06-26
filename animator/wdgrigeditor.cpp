/**
    RIGGER
    Animation editor for the DIE engine
    (c) Fred's Lab 2024-2026
    Frédéric Meslin / info@fredslab.net
    SPDX-License-Identifier: MIT
    If used commercially, contributions, donations are highly appreciated.

    2D rig editor widget
*/

#include "wdgrigeditor.h"

#include "rigger.h"
#include "mainwindow.h"

#include <QPainter>
#include <QMouseEvent>

#include <math.h>

static constexpr float D2R = 3.14159265f / 180.0f;

/*****************************************************************************/
QPen WdgRigEditor::colorPrincipal = QColor(255, 255, 255);
QPen WdgRigEditor::colorSelected = QColor(192, 192, 192);
QPen WdgRigEditor::colorJointBase = QColor(192, 32, 32);
QPen WdgRigEditor::colorBoneBase = QColor(32, 32, 255);
QPen WdgRigEditor::colorAxis = QColor(48, 48, 48);

/*****************************************************************************/
WdgRigEditor::WdgRigEditor(QWidget * parent) :
    QWidget(parent),
    scroll(),
    pressWorld(),
    selectRegionC1(), selectRegionC2(),
    selectRegion(false),
    selectRegionStart(false),
    mouseLeftWasPressed(false)
{
    setMouseTracking(true);
}

/*****************************************************************************/
QVector2D WdgRigEditor::origin() const
{
    return QVector2D(width() * 0.5f, height() * 0.5f + rigger.rigView.y * rigger.zoom());
}

QVector2D WdgRigEditor::getWorldCoordinates(const QVector2D & screen) const
{
    return (screen - origin()) / rigger.zoom();
}

/*****************************************************************************/
void WdgRigEditor::drawAxes(QPainter & painter, const QVector2D & org)
{
    painter.setPen(colorAxis);
    painter.drawLine(QPointF(org.x(), 0), QPointF(org.x(), height()));  // Y axis
    painter.drawLine(QPointF(0, org.y()), QPointF(width(), org.y()));   // ground
}

void WdgRigEditor::drawBones(QPainter & painter, const QVector2D & org)
{
    Frame * cur = rigger.rig.currentFramePtr();
    if (!cur) return;
    float zoom = rigger.zoom();

    painter.setRenderHint(QPainter::Antialiasing, true);
    for (int i = 0; i < rigger.rig.bones.count(); i++) {
        const Bone & b = rigger.rig.bones[i];
        if (b.jointID1 >= cur->joints.count()) continue;
        if (b.jointID2 >= cur->joints.count()) continue;

        QVector2D p1 = org + rigger.to2D(cur->joints[b.jointID1].pos) * zoom;
        QVector2D p2 = org + rigger.to2D(cur->joints[b.jointID2].pos) * zoom;

        if (i == rigger.selectedBone) painter.setPen(colorPrincipal);
        else if (b.selected) painter.setPen(colorSelected);
        else painter.setPen(colorBoneBase);

        painter.drawLine(p1.toPointF(), p2.toPointF());
    }
    painter.setRenderHint(QPainter::Antialiasing, false);
}

void WdgRigEditor::drawJoints(QPainter & painter, const QVector2D & org)
{
    Frame * cur = rigger.rig.currentFramePtr();
    if (!cur) return;
    float zoom = rigger.zoom();

    painter.setRenderHint(QPainter::Antialiasing, true);
    for (int pass = 0; pass < 3; pass++) {
        for (int i = 0; i < cur->joints.count(); i++) {
            const Joint & j = cur->joints[i];

            if (pass == 0) { if (j.selected || i == rigger.selectedJoint) continue; }
            else if (pass == 1) { if (!j.selected) continue; }
            else if (i != rigger.selectedJoint) continue;

            QVector2D jp = org + rigger.to2D(j.pos) * zoom;

            if (i == rigger.selectedJoint) painter.setPen(colorPrincipal);
            else if (j.selected) painter.setPen(colorSelected);
            else painter.setPen(colorJointBase);

            painter.setBrush(painter.pen().color());
            painter.drawEllipse(jp.toPointF(), 3.0, 3.0);
        }
    }
    painter.setBrush(Qt::NoBrush);
    painter.setRenderHint(QPainter::Antialiasing, false);
}

/*****************************************************************************/
void WdgRigEditor::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.fillRect(rect(), Qt::black);

    QVector2D org = origin();
    drawAxes(painter, org);
    drawBones(painter, org);
    drawJoints(painter, org);

    if (selectRegion) {
        painter.setPen(Qt::white);
        painter.setBrush(Qt::NoBrush);
        int rw = selectRegionC2.x() - selectRegionC1.x();
        int rh = selectRegionC2.y() - selectRegionC1.y();
        painter.drawRect(selectRegionC1.x(), selectRegionC1.y(), rw, rh);
    }
}

/*****************************************************************************/
void WdgRigEditor::mousePressEvent(QMouseEvent * event)
{
    QVector2D click(event->position());

// Middle button starts an orbit / vertical slide
    if (event->buttons() & Qt::MiddleButton) {
        scroll = click;
        return;
    }
    if (!(event->buttons() & Qt::LeftButton)) return;

    mouseLeftWasPressed = true;
    selectRegion = false;
    selectRegionStart = true;
    selectRegionC1 = selectRegionC2 = click;

    if (rigger.rigMode != RIG_MODE_JOINTS) return;

    bool shift = event->modifiers() & Qt::ShiftModifier;
    QVector2D world = getWorldCoordinates(click);
    pressWorld = world;

    int jId;
    if (rigger.jointFindInCircle(world, jId)) {
    // Hit a joint: select it (keep the group when shift / already selected)
        Frame * cur = rigger.rig.currentFramePtr();
        bool already = cur && jId < cur->joints.count() && cur->joints[jId].selected;
        if (!shift && !already) rigger.jointDeselectAll();
        selectRegionStart = false;
        rigger.selectedJoint = jId;
        if (cur && jId < cur->joints.count()) cur->joints[jId].selected = true;

    } else {
    // Missed: clear the selection, a region or a create may follow
        rigger.selectedJoint = RIG_UNSELECTED;
        rigger.jointDeselectAll();
    }

    if (mainWindow) mainWindow->updateJointProperties();
    update();
}

void WdgRigEditor::mouseMoveEvent(QMouseEvent * event)
{
// Middle-drag orbits the cylinder view
    if (event->buttons() & Qt::MiddleButton) {
        QVector2D p = QVector2D(event->position());
        QVector2D d = p - scroll;
        scroll = p;

        rigger.rigView.pan += d.x() * 0.5f;
        rigger.rigView.y += d.y() / rigger.zoom();
        if (mainWindow) mainWindow->updateViewerProperties();
        update();
        return;
    }

    if (!(event->buttons() & Qt::LeftButton)) return;
    if (rigger.rigMode != RIG_MODE_JOINTS) return;

    QVector2D click(event->position());

// Press landed on empty space: grow a selection rectangle
    if (selectRegionStart) {
        selectRegion = true;
        selectRegionC2 = click;
        update();
        return;
    }

// Press landed on a joint: drag the selection within the current viewing
// plane, preserving each joint's depth so it doesn't snap to the axis plane
    Frame * cur = rigger.rig.currentFramePtr();
    if (!cur) return;
    if (rigger.selectedJoint < 0 || rigger.selectedJoint >= cur->joints.count()) return;

    QVector2D world = getWorldCoordinates(click);
    float a = rigger.rigView.pan * D2R;
    float ca = cosf(a), sa = sinf(a);

    Joint & jp = cur->joints[rigger.selectedJoint];
    float depth = -jp.pos.x() * sa + jp.pos.z() * ca;
    float vx = world.x();
    QVector3D target(vx * ca - depth * sa, -world.y(), vx * sa + depth * ca);

// Translate every selected joint by the primary's delta (rigid group move)
    QVector3D delta = target - jp.pos;
    for (Joint & j : cur->joints) {
        if (!j.selected) continue;
        j.pos  += delta;
        j.apos  = j.pos;
    }

    if (mainWindow) mainWindow->updateJointProperties();
    update();
}

void WdgRigEditor::mouseReleaseEvent(QMouseEvent * event)
{
    if (mouseLeftWasPressed && rigger.rigMode == RIG_MODE_JOINTS) {
        if (!selectRegion) {
        // A plain click on empty space drops a new joint on the camera plane
            if (rigger.selectedJoint == RIG_UNSELECTED) {
                float a = rigger.rigView.pan * D2R;
                QVector3D mark(pressWorld.x() * cosf(a), -pressWorld.y(), pressWorld.x() * sinf(a));
                rigger.jointDeselectAll();
                int jNew;
                rigger.jointAdd(mark, jNew);
            }

        } else {
        // A dragged rectangle selects every joint inside it
            bool shift = event->modifiers() & Qt::ShiftModifier;
            if (!shift) rigger.jointDeselectAll();
            QVector2D c1 = getWorldCoordinates(selectRegionC1);
            QVector2D c2 = getWorldCoordinates(selectRegionC2);
            int jId;
            if (rigger.jointFindInRect(c1, c2, jId))
                rigger.selectedJoint = jId;
        }

        if (mainWindow) mainWindow->updateJointProperties();
    }

    selectRegion = false;
    selectRegionStart = false;
    mouseLeftWasPressed = false;
    update();
}

void WdgRigEditor::wheelEvent(QWheelEvent * event)
{
    constexpr float diaMin = 1.0f;
    constexpr float diaMax = 1024.0f;

    QPoint degrees = event->angleDelta();
    if (degrees.y() > 0) rigger.rigView.diameter *= 0.8f;    // zoom in -> smaller view
    if (degrees.y() < 0) rigger.rigView.diameter *= 1.25f;   // zoom out -> larger view
    if (rigger.rigView.diameter < diaMin) rigger.rigView.diameter = diaMin;
    if (rigger.rigView.diameter > diaMax) rigger.rigView.diameter = diaMax;

    if (mainWindow) mainWindow->updateViewerProperties();
    event->accept();
    update();
}
