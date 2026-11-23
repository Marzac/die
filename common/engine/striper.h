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
    \brief Low level operations on a texture strip

    A strip is a vertical stack of square power-of-two tiles, each stored
    rotated a quarter-turn, in a 32-bit (A)RGB format.

    Mipmaps chain the strip levels in one buffer: level 0 is the strip itself,
    then each level holds the same tiles at half the size, down to 1x1.
*/
class Striper
{
public:
    /// \brief Halve one size x size tile into a (size / 2) x (size / 2) one
    typedef void (*Halver)(const uint32_t * src, uint32_t * dst, int size);

    /// \brief Maximum size of an embedded strip (base64 PNG data)
    static constexpr uint64_t DataMax = 16ull << 20;

    /// \brief Convert to the 32-bit format the renderer reads
    static QImage normalize(const QImage & strip);

    /// \brief Load a strip from an image file (null on failure)
    static QImage load(const QString & file);
    /// \brief Save a strip to an image file
    static bool save(const QImage & strip, const QString & file);

    /// \brief Encode a strip as base64 PNG data, as embedded in map and rig files
    static QByteArray encode(const QImage & strip);
    /// \brief Decode base64 PNG data (null on failure)
    static QImage decode(const QByteArray & base64);
    /// \brief True when the encoded strip stays within DataMax
    static bool fits(const QImage & strip);

    /// \brief Append upright tiles of the strip size, rotating them into place
    static QImage append(const QImage & strip, const QList<QImage> & tiles);

    /// \brief Export every tile as a 3x3 mosaik PNG (concat() takes the centre tile back)
    static void slice(const QImage & strip, const QString & path);
    /// \brief Rebuild a strip from 3x3 mosaik PNGs, the inverse of slice()
    static QImage concat(const QString & path);

    /// \brief Number of mipmap levels for a tile size, level 0 included (9 for 256)
    static int levels(int size);
    /// \brief Pixel offset of a mipmap level in the chained buffer
    static size_t levelOffset(int size, int count, int level);
    /// \brief Fill the layout of a chained strip texture, all but its pixels
    static void describe(Texture & texture, int size, int count, int levels);

    /// \brief Bilinear halving, which at exactly 2:1 is the 2x2 average
    static void halveBilinear(const uint32_t * src, uint32_t * dst, int size);
    /// \brief Chain all mipmap levels of a normalized strip (empty if invalid)
    static std::vector<uint32_t> makeMipmaps(const QImage & strip, Halver halver = halveBilinear);
};

#endif
