/**
    WALLER
    Map editor for the DIE engine
    (c) Fred's Lab 2024-2026
    Frédéric Meslin / info@fredslab.net
    SPDX-License-Identifier: MIT
    If used commercially, contributions, donations are highly appreciated.

    texture strip selector widget
*/

#include "wdgtexselector.h"

#include "globals.h"
#include "editor.h"
#include "mainwindow.h"

#include <QDir>
#include <QPainter>
#include <QMouseEvent>

#include <stdint.h>

/*****************************************************************************/
WdgTexSelector::WdgTexSelector(QWidget *parent) :
    QWidget(parent),
    scroll(0)
{
}

/*****************************************************************************/
void WdgTexSelector::paintEvent(QPaintEvent *)
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

    QBrush brush = QBrush(QColor(255, 255, 255, 128));
    painter.setPen(Qt::NoPen);
    painter.setBrush(brush);

    int x = editor.selectedTextureID * tileWidth - offset;
    QRect selection = QRect(x, 0, tileWidth, tileWidth);
    painter.drawRect(selection);
}

/*****************************************************************************/
void WdgTexSelector::mousePressEvent(QMouseEvent *event)
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
void WdgTexSelector::setScroll(int scroll)
{
    this->scroll = scroll;
    update();
}

/*****************************************************************************/
void WdgTexSelector::wheelEvent(QWheelEvent *event)
{
    QPoint degrees = event->angleDelta();
    if (degrees.y() > 0) scroll -= 25;
    if (degrees.y() < 0) scroll += 25;
    scroll = qBound(0, scroll, 999);
    event->accept();
    update();
}

/*****************************************************************************/
// Export every tile as a 3x3 mosaik (concat() takes the centre tile back)
void WdgTexSelector::slice(const QString & path)
{
    QImage & strip = editor.editedMap->textures;
    int size = strip.width();
    int count = strip.height() / size;

    QDir().mkpath(path); // Ensure output directory exists

    for (int i = 0; i < count; ++i) {
        QRect tileRect(0, i * size, size, size);
        QImage tile = strip.copy(tileRect);

        QImage mosaik(size * 3, size * 3, QImage::Format_ARGB32);
        mosaik.fill(Qt::transparent);

        QPainter painter(&mosaik);
        for (int y = 0; y < 3; y++)
            for (int x = 0; x < 3; x++)
                painter.drawImage(x * size, y * size, tile);
        painter.end();

        QString outputFile = QString("%1/tile_%2.png").arg(path).arg(i, 2, 10, QChar('0'));
        mosaik.save(outputFile);
    }
}

// Rebuild the strip from 3x3 mosaik PNGs, the inverse of slice()
void WdgTexSelector::concat(const QString & path)
{
    QDir dir(path);
    QStringList nameFilters = {"*.png"};
    QStringList fileList = dir.entryList(nameFilters, QDir::Files, QDir::Name);
    if (fileList.isEmpty()) return;

    QList<QImage> images;

    for (const QString & fileName : fileList) {
        QImage img(dir.filePath(fileName));
        if (img.isNull()) continue;
        images.append(img);
    }
    if (images.isEmpty()) return;

    int size = images.first().width() / 3;
    int stripWidth = size;
    int stripHeight = size * images.count();

    QImage strip(stripWidth, stripHeight, QImage::Format_ARGB32);
    strip.fill(Qt::transparent);

    QPainter painter(&strip);
    for (int i = 0; i < images.count(); i++)
        painter.drawImage(0, i * size, images[i], size, size, size, size);
    painter.end();

    strip.save(path + "/fullstrip.png");
    editor.editedMap->textures = strip;
}
