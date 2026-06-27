/**
    RIGGER
    Animation editor for the DIE engine
    (c) Fred's Lab 2024-2026
    Frédéric Meslin / info@fredslab.net
    SPDX-License-Identifier: MIT
    If used commercially, contributions, donations are highly appreciated.

    texture strip selector widget
*/

#include "wdgtexselector.h"

#include "rigger.h"
#include "mainwindow.h"

#include <QPainter>
#include <QMouseEvent>

#include <stdint.h>

/*****************************************************************************/
WdgTexSelector::WdgTexSelector(QWidget * parent) :
    QWidget(parent),
    scroll(0)
{
}

/*****************************************************************************/
void WdgTexSelector::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.fillRect(rect(), Qt::black);
    painter.setPen(QPen(Qt::black));

    QImage & strip = rigger.rig.textures;
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

    int cursor = 0;
    for (int i = start; i < stop; i++) {
        QRect source = QRect(0, i * texWidth, texWidth, texWidth);
        QRect target = QRect(cursor++ * tileWidth, 0, tileWidth, tileWidth);
        painter.drawImage(target, strip, source);
    }

    QBrush brush = QBrush(QColor(255, 255, 255, 128));
    painter.setPen(Qt::NoPen);
    painter.setBrush(brush);

    int x = rigger.selectedTextureID * tileWidth - offset;
    QRect selection = QRect(x, 0, tileWidth, tileWidth);
    painter.drawRect(selection);
}

/*****************************************************************************/
void WdgTexSelector::mousePressEvent(QMouseEvent * event)
{
    QImage & strip = rigger.rig.textures;

    int texWidth = strip.width();
    int tileWidth = height();
    if (texWidth <= 0 || tileWidth <= 0) return;

    int texCount = strip.height() / texWidth;
    int overflow = texCount * tileWidth - width();
    if (overflow < 0) overflow = 0;
    int offset = (overflow * scroll) / 1000;

    int texId = (event->position().x() + offset) / tileWidth;
    if (texId < 0 || texId >= texCount) return;

    rigger.selectedTextureID = (uint16_t) texId;
    if (mainWindow) mainWindow->setTexture((uint16_t) texId);
    update();
}

/*****************************************************************************/
void WdgTexSelector::setScroll(int scroll)
{
    this->scroll = scroll;
    update();
}

/*****************************************************************************/
void WdgTexSelector::wheelEvent(QWheelEvent * event)
{
    QPoint degrees = event->angleDelta();
    if (degrees.y() > 0) scroll -= 25;
    if (degrees.y() < 0) scroll += 25;
    scroll = qBound(0, scroll, 999);
    event->accept();
    update();
}
