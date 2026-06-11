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
#include <QMouseEvent>

#include <math.h>

/*****************************************************************************/
WdgMapView::WdgMapView(QWidget *parent) :
    QWidget(parent), clickRegistered(false)
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

// Handle selection: the click was picked by the render
    if (!clickRegistered) return;
    clickRegistered = false;

    uint16_t wid = renderer.clickGetWallID();
    if (wid == Renderer::ClickNoWall) return;

// Update the editor
    editor.wallDeselectAll();
    editor.wallSelect(wid);
    mainWindow->updateWallProperties();
}

/*****************************************************************************/
void WdgMapView::mousePressEvent(QMouseEvent *event)
{
    QImage * frame = renderer.getImage();
    if (!frame) return;

    dragX = event->pos().x();

    int offsetX = (width() - frame->width()) / 2;
    int offsetY = (height() - frame->height()) / 2;
    int realX = dragX - offsetX;
    int realY = event->pos().y() - offsetY;

    renderer.clickRegister(realX, realY);
    clickRegistered = true;
}

/*****************************************************************************/
void WdgMapView::mouseMoveEvent(QMouseEvent *event)
{
    constexpr float panSensitivity = 0.9f;
    constexpr float strafeSensitivity = 0.250f;
    constexpr float tiltSpan = 80.0f;

    if (!event->buttons()) return;

    int deltaX = event->pos().x() - dragX;
    dragX = event->pos().x();

    if (event->buttons() & Qt::LeftButton) {
        float pan = walker.pan + deltaX * panSensitivity;
        walker.pan = fmodf(pan, 360.0f);
    }

    if (event->buttons() & Qt::MiddleButton) {
        float pan = walker.pan;
        QVector3D dir(-sinf(D2R * pan), 0.0f, cosf(D2R * pan));
        walker.pos += dir * deltaX * strafeSensitivity;
    }

    int deltaY = event->pos().y() - height() / 2;
    float pitch = -tiltSpan * deltaY / (float) height();
    walker.pitchTargetMouse = pitch;
}

void WdgMapView::wheelEvent(QWheelEvent *event)
{
    constexpr float scrollAdvance = 0.0625f;
    float pan = walker.pan;
    QVector3D dir(cosf(D2R * pan), 0.0f, sinf(D2R * pan));
    walker.pos += dir * event->angleDelta().y() * scrollAdvance;
}
