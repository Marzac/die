/**
    DIE ENGINE
    Depth Integration Engine / A modern ray-caster
    (c) Fred's Lab 2024-2026
    Frédéric Meslin / info@fredslab.net
    SPDX-License-Identifier: MIT
    If used commercially, contributions, donations are highly appreciated.

    texture strip helpers
*/

#ifndef STRIPER_H
#define STRIPER_H

#include "primitives.h"

#include <QImage>
#include <QList>
#include <QByteArray>
#include <QString>

#include <vector>
#include <stddef.h>
#include <stdint.h>

/*****************************************************************************/
/**
    \brief Texture strip operations

    A strip stacks square power-of-two tiles, rotated a quarter-turn, in 32-bit (A)RGB.
    Mipmaps chain its levels in one buffer: level 0, then halved down to 1x1.
    Key color texels are holes, kept through the levels.
*/
class Striper
{
public:
    /// \brief Halve a size x size tile
    typedef void (*Halver)(const uint32_t * src, uint32_t * dst, int size);

    /// \brief Embedded strip size limit (base64 PNG)
    static constexpr uint64_t DataMax = 16ull << 20;

    /// \brief Convert to 32-bit (A)RGB
    static QImage normalize(const QImage & strip);

    /// \brief Load an image file (null on failure)
    static QImage load(const QString & file);
    /// \brief Save to an image file
    static bool save(const QImage & strip, const QString & file);

    /// \brief Encode as base64 PNG, as embedded in map and rig files
    static QByteArray encode(const QImage & strip);
    /// \brief Decode base64 PNG (null on failure)
    static QImage decode(const QByteArray & base64);
    /// \brief True if the encoded strip fits in DataMax
    static bool fits(const QImage & strip);

    /// \brief Append upright tiles, rotated into place
    static QImage append(const QImage & strip, const QList<QImage> & tiles);

    /// \brief Export each tile as a 3x3 mosaik PNG
    static void slice(const QImage & strip, const QString & path);
    /// \brief Rebuild a strip from mosaik PNGs, inverse of slice()
    static QImage concat(const QString & path);

    /// \brief Mipmap levels for a tile size, level 0 included
    static int levels(int size);
    /// \brief Pixel offset of a mipmap level in the chained buffer
    static size_t levelOffset(int size, int count, int level);
    /// \brief Fill a texture layout, all but its pixels
    static void describe(Texture & texture, int size, int count, int levels);

    /// \brief Bilinear halving: the 2x2 average
    static void halveBilinear(const uint32_t * src, uint32_t * dst, int size);
    /// \brief Chain the mipmap levels of a normalized strip (empty if invalid)
    static std::vector<uint32_t> makeMipmaps(const QImage & strip, Halver halver = halveBilinear);
};

#endif
