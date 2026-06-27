/**
    RIGGER
    Animation editor for the DIE engine
    (c) Fred's Lab 2024-2026
    Frédéric Meslin / info@fredslab.net
    SPDX-License-Identifier: MIT
    If used commercially, contributions, donations are highly appreciated.

    animation frame selector widget
*/

#include "wdgframeselector.h"

#include "rigger.h"
#include "mainwindow.h"

#include <QPainter>
#include <QMouseEvent>

/*****************************************************************************/
WdgFrameSelector::WdgFrameSelector(QWidget * parent) :
    QWidget(parent),
    scroll(0)
{
}

/*****************************************************************************/
void WdgFrameSelector::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.fillRect(rect(), Qt::black);

    Animation * anim = rigger.rig.currentAnimationPtr();
    if (!anim) return;

    int count = anim->frames.count();
    int cardWidth = height();
    if (count <= 0 || cardWidth <= 0) return;

    int cardSpace = width() / cardWidth;
    int overflow = count * cardWidth - width();
    if (overflow < 0) overflow = 0;
    int offset = (overflow * scroll) / 1000;

    int start = offset / cardWidth;
    int stop = start + cardSpace + 2;
    if (stop > count) stop = count;
    int shift = offset % cardWidth;

    QFont font = painter.font();
    font.setBold(true);
    painter.setFont(font);

    for (int i = start; i < stop; i++) {
        int x = (i - start) * cardWidth - shift;
        QRect card(x, 0, cardWidth, cardWidth);

        bool isCurrent = (i == rigger.rig.currentFrame);
        painter.setPen(QPen(Qt::white));
        painter.setBrush(isCurrent ? QColor(96, 96, 96) : QColor(48, 48, 48));
        painter.drawRect(card.adjusted(1, 1, -2, -2));

        painter.setPen(isCurrent ? Qt::white : QColor(160, 160, 160));
        painter.drawText(card, Qt::AlignCenter, QString::number(i));
    }
}

/*****************************************************************************/
void WdgFrameSelector::mousePressEvent(QMouseEvent * event)
{
    Animation * anim = rigger.rig.currentAnimationPtr();
    if (!anim) return;

    int count = anim->frames.count();
    int cardWidth = height();
    if (count <= 0 || cardWidth <= 0) return;

    int overflow = count * cardWidth - width();
    if (overflow < 0) overflow = 0;
    int offset = (overflow * scroll) / 1000;

    int fId = (event->position().x() + offset) / cardWidth;
    if (fId < 0 || fId >= count) return;

    rigger.rig.frameSelect(fId);
    if (mainWindow) {
        mainWindow->updateJointProperties();
        mainWindow->updateRigCanvas();
    }
    update();
}

/*****************************************************************************/
void WdgFrameSelector::setScroll(int scroll)
{
    this->scroll = scroll;
    update();
}

/*****************************************************************************/
void WdgFrameSelector::wheelEvent(QWheelEvent * event)
{
    QPoint degrees = event->angleDelta();
    if (degrees.y() > 0) scroll -= 25;
    if (degrees.y() < 0) scroll += 25;
    scroll = qBound(0, scroll, 999);
    event->accept();
    update();
}
