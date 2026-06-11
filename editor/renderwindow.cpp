/**
    WALLER
    Map editor for the DIE engine
    (c) Fred's Lab 2024-2026
    Frédéric Meslin / info@fredslab.net
    SPDX-License-Identifier: MIT
    If used commercially, contributions, donations are highly appreciated.

    render output window
*/

#include "renderwindow.h"
#include "ui_renderwindow.h"

#include "renderer.h"

#include <QPainter>

/*****************************************************************************/
RenderWindow::RenderWindow(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::RenderWindow)
{
    ui->setupUi(this);
    setWindowFlags(Qt::Window | Qt::FramelessWindowHint);
    setMouseTracking(true);
}

RenderWindow::~RenderWindow()
{
    delete ui;
}

/*****************************************************************************/
void RenderWindow::paintEvent(QPaintEvent *)
{
    QImage * frame = renderer.getImage();
    if (!frame) return;

    QPainter painter(this);
    int offsetX = (width() - frame->width()) / 2;
    int offsetY = (height() - frame->height()) / 2;
    painter.drawImage(offsetX, offsetY, *frame);
}
