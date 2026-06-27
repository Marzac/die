/**
    RIGGER
    Animation editor for the DIE engine
    (c) Fred's Lab 2024-2026
    Frédéric Meslin / info@fredslab.net
    SPDX-License-Identifier: MIT
    If used commercially, contributions, donations are highly appreciated.

    texture preview widget
*/

#include "wdgtexview.h"

#include "rigger.h"

#include <QPainter>

/*****************************************************************************/
WdgTexView::WdgTexView(QWidget * parent) :
    QWidget(parent),
    id(0)
{
}

/*****************************************************************************/
void WdgTexView::setID(uint16_t id)
{
    this->id = id;
    update();
}

/*****************************************************************************/
void WdgTexView::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.fillRect(rect(), Qt::black);

    QImage & strip = rigger.rig.textures;
    int size = strip.width();
    if (size <= 0) return;

    painter.setRenderHint(QPainter::Antialiasing);
    QRect source = QRect(0, id * size, size, size);
    painter.drawImage(rect(), strip, source);
}
