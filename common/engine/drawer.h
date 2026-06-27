/**
    RIGGER
    Animation editor for the DIE engine
    (c) Fred's Lab 2024-2026
    Frédéric Meslin / info@fredslab.net
    SPDX-License-Identifier: MIT
    If used commercially, contributions, donations are highly appreciated.

    rigger model
*/

#ifndef DRAWER_H
#define DRAWER_H

    #include "primitives.h"

    #include <QImage>

    #include <stdint.h>

    // Vertex UVs (us / vs) are normalised: 0.0 .. 1.0 spans one texture tile.
    void drawerQuad(QImage & image, const Quad * quad, const Texture & texture);
    void drawerTriangle(QImage & image, const Triangle * triangle, const Texture & texture);

#endif //DRAWER_H