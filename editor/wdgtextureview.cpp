/**
    WALLER
    Map editor for the DIE engine
    (c) Fred's Lab 2024-2026
    Frédéric Meslin / info@fredslab.net
    SPDX-License-Identifier: MIT
    If used commercially, contributions, donations are highly appreciated.

    texture preview widget
*/

#include "wdgtextureview.h"

#include "editor.h"

#include <QPainter>

/*****************************************************************************/
WdgTextureView::WdgTextureView(QWidget *parent) :
    QWidget(parent),
    id(0)
{
}

/*****************************************************************************/
void WdgTextureView::setID(uint16_t id)
{
    this->id = id;
    update();
}

/*****************************************************************************/
void WdgTextureView::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    QImage & strip = editor.editedMap->textures;
    QRect target = QRect(-width(), 0, width(), height());
    QRect source = QRect(0, id * strip.width(), strip.width(), strip.width());

    painter.save();
    painter.rotate(-90);
    painter.drawImage(target, strip, source);
    painter.restore();
}
