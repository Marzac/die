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

#include <emmintrin.h>
#include <algorithm>
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

/*****************************************************************************/
int Striper::levels(int size)
{
    int n = 1;
    while (size > 1 && !(size & 1)) {size >>= 1; n++;}
    return n;
}

size_t Striper::levelOffset(int size, int count, int level)
{
    size_t offset = 0;
    for (int l = 0; l < level; l++) {
        size_t s = (size_t) (size >> l);
        offset += s * s * count;
    }
    return offset;
}

void Striper::describe(Texture & texture, int size, int count, int levels)
{
    texture.size = (uint16_t) size;
    texture.count = (uint16_t) count;
    texture.mask = (uint16_t) (size - 1);
    texture.block = (uint32_t) (size * size);
    texture.mmLevels = (uint16_t) std::clamp(levels, 1, TEXTURE_LEVELS_MAX);
    for (int l = 0; l < texture.mmLevels; l++)
        texture.mmOffsets[l] = (uint32_t) levelOffset(size, count, l);
}

/*****************************************************************************/
void Striper::halveBilinear(const uint32_t * src, uint32_t * dst, int size)
{
    const __m128i zero = _mm_setzero_si128();
    const __m128i round = _mm_set1_epi16(2);
    int half = size >> 1;

    for (int y = 0; y < half; y++) {
        const uint32_t * r0 = src + 2 * y * size;
        const uint32_t * r1 = r0 + size;
        uint32_t * d = dst + y * half;

    // Two output pixels per pass, summed in 16-bit lanes
        int x = 0;
        for (; x + 2 <= half; x += 2) {
            __m128i a = _mm_loadu_si128((const __m128i *) (r0 + 2 * x));
            __m128i b = _mm_loadu_si128((const __m128i *) (r1 + 2 * x));
            __m128i lo = _mm_add_epi16(_mm_unpacklo_epi8(a, zero), _mm_unpacklo_epi8(b, zero));
            __m128i hi = _mm_add_epi16(_mm_unpackhi_epi8(a, zero), _mm_unpackhi_epi8(b, zero));
            lo = _mm_add_epi16(lo, _mm_srli_si128(lo, 8));
            hi = _mm_add_epi16(hi, _mm_srli_si128(hi, 8));
            __m128i sum = _mm_srli_epi16(_mm_add_epi16(_mm_unpacklo_epi64(lo, hi), round), 2);
            _mm_storel_epi64((__m128i *) (d + x), _mm_packus_epi16(sum, zero));
        }

    // Last odd pixel (2x2 to 1x1)
        for (; x < half; x++) {
            uint32_t p0 = r0[2 * x], p1 = r0[2 * x + 1], p2 = r1[2 * x], p3 = r1[2 * x + 1];
            uint32_t out = 0;
            for (int c = 0; c < 32; c += 8) {
                uint32_t sum = ((p0 >> c) & 0xFF) + ((p1 >> c) & 0xFF) + ((p2 >> c) & 0xFF) + ((p3 >> c) & 0xFF) + 2;
                out |= (sum >> 2) << c;
            }
            d[x] = out;
        }
    }
}

/*****************************************************************************/
static bool hasKey(const uint32_t * tile, size_t pixels)
{
    for (size_t i = 0; i < pixels; i++)
        if (tile[i] == TEXTURE_KEY_COLOR) return true;
    return false;
}

// Key texels take the average of their opaque neighbours, so no key blends into the edges
static void padKeys(const uint32_t * tile, uint32_t * padded, int size)
{
    int mask = size - 1;
    for (int y = 0; y < size; y++)
        for (int x = 0; x < size; x++) {
            uint32_t p = tile[y * size + x];
            if (p != TEXTURE_KEY_COLOR) {padded[y * size + x] = p; continue;}

            uint32_t sum[4] = {0, 0, 0, 0};
            uint32_t n = 0;
            for (int dy = -1; dy <= 1; dy++)
                for (int dx = -1; dx <= 1; dx++) {
                    uint32_t q = tile[((y + dy) & mask) * size + ((x + dx) & mask)];   // tiles wrap
                    if (q == TEXTURE_KEY_COLOR) continue;
                    for (int c = 0; c < 4; c++) sum[c] += (q >> (8 * c)) & 0xFF;
                    n++;
                }

        // No opaque neighbour: its 2x2 block is all key, a hole anyway
            uint32_t out = 0;
            if (n) for (int c = 0; c < 4; c++) out |= ((sum[c] + n / 2) / n) << (8 * c);
            padded[y * size + x] = out;
        }
}

// A 2x2 block with 3 or 4 key texels stays a hole
static void voteKeys(const uint32_t * tile, uint32_t * dst, int size)
{
    int half = size >> 1;
    for (int y = 0; y < half; y++)
        for (int x = 0; x < half; x++) {
            const uint32_t * p = tile + 2 * y * size + 2 * x;
            int keys = (p[0] == TEXTURE_KEY_COLOR) + (p[1] == TEXTURE_KEY_COLOR)
                     + (p[size] == TEXTURE_KEY_COLOR) + (p[size + 1] == TEXTURE_KEY_COLOR);
            if (keys >= 3) dst[y * half + x] = TEXTURE_KEY_COLOR;
        }
}

/*****************************************************************************/
std::vector<uint32_t> Striper::makeMipmaps(const QImage & strip, Halver halver)
{
    std::vector<uint32_t> buffer;
    int size = strip.width();
    if (strip.isNull() || size <= 0 || strip.depth() != 32) return buffer;
    int count = strip.height() / size;
    if (!count) return buffer;

    int n = levels(size);
    buffer.resize(levelOffset(size, count, n));

// Level 0 is the strip, each next level halves the previous one
    size_t block = (size_t) size * size * count;
    memcpy(buffer.data(), strip.constBits(), block * sizeof(uint32_t));

    std::vector<uint32_t> padded((size_t) size * size);
    const uint32_t * src = buffer.data();
    uint32_t * dst = buffer.data() + block;
    for (int l = 1, s = size; l < n; l++, s >>= 1) {
        size_t srcTile = (size_t) s * s;
        size_t dstTile = srcTile / 4;
        for (int i = 0; i < count; i++) {
            const uint32_t * tile = src + i * srcTile;
            uint32_t * out = dst + i * dstTile;
        // Masked tile: halve it padded, then put the holes back
            if (hasKey(tile, srcTile)) {
                padKeys(tile, padded.data(), s);
                halver(padded.data(), out, s);
                voteKeys(tile, out, s);
            } else halver(tile, out, s);
        }
        src = dst;
        dst += dstTile * count;
    }
    return buffer;
}
