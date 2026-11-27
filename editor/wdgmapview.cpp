/**
    WALLER
    Map editor for the DIE engine
    (c) Fred's Lab 2024-2026
    Frédéric Meslin / info@fredslab.net
    SPDX-License-Identifier: MIT
    If used commercially, contributions, donations are highly appreciated.

    3D map view widget
*/

#include "wdgmapview.h"

#include "globals.h"
#include "editor.h"
#include "mainwindow.h"
#include "renderer.h"
#include "walker.h"

#include <QPainter>
#include <QApplication>

#include <math.h>

/*****************************************************************************/
void ViewMouse::press(QMouseEvent * event)
{
    start = last = event->pos();
    dragged = false;
}

void ViewMouse::move(QMouseEvent * event)
{
    if (!event->buttons()) return;
    if ((event->pos() - start).manhattanLength() >= QApplication::startDragDistance())
        dragged = true;

    float deltaX = event->pos().x() - last.x();
    float deltaY = event->pos().y() - last.y();
    last = event->pos();

    if (event->modifiers() & Qt::ShiftModifier) {
        deltaX *= 2.0f;
        deltaY *= 2.0f;
    }

    if (event->modifiers() & Qt::AltModifier) {
        deltaX *= 0.5f;
        deltaY *= 0.5f;
    }

    if (event->buttons() & Qt::LeftButton) {
        float pan = walker.pan + deltaX * panSensitivity;
        walker.pan = remainderf(pan, 360.0f);
        float pitch = walker.pitchTargetMouse - pitchSensitivity * deltaY;
        walker.pitchTargetMouse = std::clamp(pitch, -pitchSpanMax, pitchSpanMax);
    }

    if (event->buttons() & Qt::MiddleButton) {
        float pan = walker.pan;
        QVector3D dirUp(0.0f, 1.0f, 0.0f);
        QVector3D dirRight(-sinf(D2R * pan), 0.0f, cosf(D2R * pan));
        walker.pos += dirRight * deltaX * strafeSensitivity;
        walker.pos += dirUp * deltaY * strafeSensitivity;
        walker.pitchTargetMouse = 0;
    }
}

void ViewMouse::release(const QWidget * view, QMouseEvent * event)
{
// Pick on a left click without drag
    if (event->button() != Qt::LeftButton || dragged) return;
    if (!editor.wallSelector) return;
    QImage * frame = renderer.getImage();
    if (!frame) return;

    int x = event->pos().x() - (view->width() - frame->width()) / 2;
    int y = event->pos().y() - (view->height() - frame->height()) / 2;
    renderer.clickRegister(x, y);
    mainWindow->wallPickPending = true;
    mainWindow->wallPickAdd = event->modifiers() & Qt::ShiftModifier;
}

void ViewMouse::wheel(QWheelEvent * event)
{
    float pan = walker.pan;
    float walk = event->angleDelta().y();
    if (event->modifiers() & Qt::ShiftModifier) walk *= 2.0f;
    if (event->modifiers() & Qt::AltModifier) walk *= 0.5f;

    QVector3D dir(cosf(D2R * pan), 0.0f, sinf(D2R * pan));
    walker.pos += dir * walk * walkSensitivity;
    walker.pitchTargetMouse = 0;
}

/*****************************************************************************/
WdgMapView::WdgMapView(QWidget *parent) :
    QWidget(parent)
{
    setMouseTracking(true);
}

/*****************************************************************************/
void WdgMapView::drawViewLabel(QPainter & painter)
{
    QFont font = painter.font();
    font.setBold(true);
    font.setPointSize(12);
    painter.setFont(font);
    painter.setPen(Qt::white);

    QRect textRect(10, 10, width() - 20, 30);
    painter.drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter, "3D");
}

/*****************************************************************************/
void WdgMapView::paintEvent(QPaintEvent *)
{
    QImage * frame = renderer.getImage();
    if (!frame) return;
    if (!editor.editedMap) return;

    QPainter painter(this);
    int offsetX = (width() - frame->width()) / 2;
    int offsetY = (height() - frame->height()) / 2;
    painter.drawImage(offsetX, offsetY, *frame);
}
