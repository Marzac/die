/**
    DIE ENGINE
    Depth Integration Engine / A modern ray-caster
    (c) Fred's Lab 2024-2026
    Frédéric Meslin / info@fredslab.net
    SPDX-License-Identifier: MIT
    If used commercially, contributions, donations are highly appreciated.

    texture strip helpers
*/

#include "striper.h"

#include <QBuffer>
#include <QColorSpace>
#include <QDir>
#include <QPainter>
#include <QTransform>

#include <cstring>

/*****************************************************************************/
QImage Striper::normalize(const QImage & strip)
{
    QImage::Format f = strip.format();
    if (strip.isNull() || f == QImage::Format_RGB32 || f == QImage::Format_ARGB32) return strip;
    return strip.convertToFormat(strip.hasAlphaChannel() ? QImage::Format_ARGB32 : QImage::Format_RGB32);
}

/*****************************************************************************/
QImage Striper::load(const QString & file)
{
    return normalize(QImage(file));
}

bool Striper::save(const QImage & strip, const QString & file)
{
    return strip.save(file);
}

/*****************************************************************************/
QByteArray Striper::encode(const QImage & strip)
{
    QByteArray png;
    QBuffer buffer(&png);
    buffer.open(QIODevice::WriteOnly);
    strip.save(&buffer, "PNG");
    return png.toBase64();
}

QImage Striper::decode(const QByteArray & base64)
{
    return normalize(QImage::fromData(QByteArray::fromBase64(base64), "PNG"));
}

bool Striper::fits(const QImage & strip)
{
    return (uint64_t) encode(strip).size() <= DataMax;
}

/*****************************************************************************/
QImage Striper::append(const QImage & strip, const QList<QImage> & tiles)
{
    if (tiles.isEmpty()) return strip;
    int size = strip.isNull() ? tiles.first().width() : strip.width();
    int count = strip.isNull() ? 0 : strip.height() / size;

    bool alpha = strip.hasAlphaChannel();
    for (const QImage & t : tiles) alpha |= t.hasAlphaChannel();
    QImage grown(size, (count + tiles.count()) * size, alpha ? QImage::Format_ARGB32 : QImage::Format_RGB32);
    if (!strip.isNull()) grown.setColorSpace(strip.colorSpace());

    auto copy = [&](const QImage & src, int rows, int y0) {
        QImage s = src.convertToFormat(grown.format());
        for (int y = 0; y < rows; y++)
            memcpy(grown.scanLine(y0 + y), s.constScanLine(y), size * sizeof(uint32_t));
    };
    if (count) copy(strip, count * size, 0);

// Tiles are stored rotated a quarter-turn
    for (int i = 0; i < tiles.count(); i++)
        copy(tiles[i].transformed(QTransform().rotate(90)), size, (count + i) * size);
    return grown;
}

/*****************************************************************************/
void Striper::slice(const QImage & strip, const QString & path)
{
    int size = strip.width();
    if (size <= 0) return;
    int count = strip.height() / size;

    QDir().mkpath(path);

    for (int i = 0; i < count; ++i) {
        QImage tile = strip.copy(QRect(0, i * size, size, size));

        QImage mosaik(size * 3, size * 3, QImage::Format_ARGB32);
        mosaik.fill(Qt::transparent);

        QPainter painter(&mosaik);
        for (int y = 0; y < 3; y++)
            for (int x = 0; x < 3; x++)
                painter.drawImage(x * size, y * size, tile);
        painter.end();

        mosaik.save(QString("%1/tile_%2.png").arg(path).arg(i, 2, 10, QChar('0')));
    }
}

QImage Striper::concat(const QString & path)
{
    QDir dir(path);
    QStringList fileList = dir.entryList({"*.png"}, QDir::Files, QDir::Name);

    QList<QImage> images;
    for (const QString & fileName : fileList) {
        QImage img(dir.filePath(fileName));
        if (!img.isNull()) images.append(img);
    }
    if (images.isEmpty()) return QImage();

    int size = images.first().width() / 3;
    QImage strip(size, size * images.count(), QImage::Format_ARGB32);
    strip.fill(Qt::transparent);

    QPainter painter(&strip);
    for (int i = 0; i < images.count(); i++)
        painter.drawImage(0, i * size, images[i], size, size, size, size);
    painter.end();

    strip.save(path + "/fullstrip.png");
    return strip;
}
