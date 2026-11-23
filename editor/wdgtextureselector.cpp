/**
    WALLER
    Map editor for the DIE engine
    (c) Fred's Lab 2024-2026
    Frédéric Meslin / info@fredslab.net
    SPDX-License-Identifier: MIT
    If used commercially, contributions, donations are highly appreciated.

    texture strip selector widget
*/

#include "wdgtextureselector.h"

#include "globals.h"
#include "editor.h"
#include "mainwindow.h"

#include <QPainter>
#include <QMouseEvent>

#include <stdint.h>

/*****************************************************************************/
WdgTextureSelector::WdgTextureSelector(QWidget *parent) :
    QWidget(parent),
    scroll(0)
{
}

/*****************************************************************************/
void WdgTextureSelector::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setPen(QPen(Qt::black));

    QImage & strip = editor.editedMap->textures;
    int texWidth = strip.width();
    int tileWidth = height();
    if (texWidth <= 0 || tileWidth <= 0) return;

    int texCount = strip.height() / texWidth;
    int tileSpace = width() / tileWidth;

    int overflow = texCount * tileWidth - width();
    if (overflow < 0) overflow = 0;
    int offset = (overflow * scroll) / 1000;

    int start = offset / tileWidth;
    int stop = start + tileSpace + 2;
    if (stop > texCount) stop = texCount;
    int shift = offset % tileWidth;

    painter.save();
    painter.rotate(-90);
    int cursor = 0;
    for (int i = start; i < stop; i++) {
        QRect source = QRect(0, i * texWidth, texWidth, texWidth);
        QRect target = QRect(-tileWidth, cursor++ * tileWidth - shift, tileWidth, tileWidth);
        painter.drawImage(target, strip, source);
    }
    painter.restore();

// Light tint and a frame, so the texture stays visible
    painter.setPen(QPen(QColor(255, 255, 255, 192), 2));
    painter.setBrush(QColor(255, 255, 255, 40));

    int x = editor.selectedTextureID * tileWidth - offset;
    QRect selection = QRect(x + 1, 1, tileWidth - 2, tileWidth - 2);
    painter.drawRect(selection);
}

/*****************************************************************************/
void WdgTextureSelector::mousePressEvent(QMouseEvent *event)
{
    QImage & strip = editor.editedMap->textures;

    int texWidth = strip.width();
    int tileWidth = height();
    if (texWidth <= 0 || tileWidth <= 0) return;

    int texCount = strip.height() / texWidth;
    int overflow = texCount * tileWidth - width();
    if (overflow < 0) overflow = 0;
    int offset = (overflow * scroll) / 1000;

    int texId = (event->position().x() + offset) / tileWidth;
    if (texId < 0 || texId >= texCount) return;
    mainWindow->setTexture((uint16_t) texId);
    editor.selectedTextureID = (uint16_t) texId;
    update();
}

/*****************************************************************************/
void WdgTextureSelector::setScroll(int scroll)
{
    this->scroll = scroll;
    update();
}

/*****************************************************************************/
void WdgTextureSelector::wheelEvent(QWheelEvent *event)
{
    QPoint degrees = event->angleDelta();
    if (degrees.y() > 0) scroll -= 25;
    if (degrees.y() < 0) scroll += 25;
    scroll = qBound(0, scroll, 999);
    event->accept();
    update();
}
