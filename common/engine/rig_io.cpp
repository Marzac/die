/**
    DIE ENGINE
    Depth Integration Engine / A modern ray-caster
    (c) Fred's Lab 2024-2026
    Frédéric Meslin / info@fredslab.net
    SPDX-License-Identifier: MIT
    If used commercially, contributions, donations are highly appreciated.

    rig load/save
*/

#include "rig.h"

#include <QBuffer>
#include <QDebug>

#include <stdio.h>
#include <stdint.h>
#include <string.h>

// Maximum size of an embedded texture strip (base64 data)
static constexpr uint64_t TEXTURE_DATA_MAX = 16ull << 20;

/*****************************************************************************/
static void skipLine(FILE * file)
{
    while (true) {
        int c = fgetc(file);
        if (c == EOF) return;
        if (c == '\n') return;
    }
}

/*****************************************************************************/
bool Rig::save(const QString & filename)
{
    FILE * file = fopen(filename.toLocal8Bit().constData(), "wb");
    if (!file) return false;

    fprintf(file, "# == DIE RIG == \n");
    fprintf(file, "# Fred's Lab 2024-2026\n");
    fprintf(file, "# 26.06.26 - V1.0\n");

// ==== BONES ====
    fprintf(file, "# == BONES ==\n");
    fprintf(file, "# format: B jointID1, jointID2, width, length, offset, minWidth, imageCount, flags\n");
    fprintf(file, "# format: I index, imageID\n");
    for (const Bone & b : bones) {
        fprintf(file, "B %04hu, %04hu, %+4.4f, %+4.4f, %+4.4f, %+4.4f, %02hu, %04hx\n",
            b.jointID1, b.jointID2, b.width, b.length, b.offset, b.minWidth, b.imageCount, b.flags);
        uint16_t count = b.imageCount > RIG_BONE_IMAGES_MAX ? RIG_BONE_IMAGES_MAX : b.imageCount;
        for (int i = 0; i < count; i++)
            fprintf(file, "\tI %02d, %04hu\n", i, b.images[i]);
    }
    fprintf(file, "\n");

// ==== ANIMATIONS ====
    fprintf(file, "# == ANIMATIONS ==\n");
    fprintf(file, "# format: A name\n");
    fprintf(file, "# format: F index\n");
    fprintf(file, "# format: J x, y, z, flags\n");
    for (const Animation & a : animations) {
        const char * name = a.name[0] ? a.name : "None";
        fprintf(file, "A %.31s\n", name);
        for (int f = 0; f < a.frames.count(); f++) {
            fprintf(file, "\tF %04d\n", f);
            for (const Joint & j : a.frames[f].joints)
                fprintf(file, "\t\tJ %+4.4f, %+4.4f, %+4.4f, %04hx\n",
                    j.pos.x(), j.pos.y(), j.pos.z(), j.flags);
        }
    }
    fprintf(file, "\n");

// ==== TEXTURE STRIP ====
    fprintf(file, "# == TEXTURE STRIP ==\n");
    fprintf(file, "# format: T type, length, data\n");

    QByteArray imageData;
    QBuffer buffer(&imageData);
    buffer.open(QIODevice::WriteOnly);
    textures.save(&buffer, "PNG");
    QByteArray base64Data = imageData.toBase64();

    fprintf(file, "T %02d, %llx\n", 0, (unsigned long long) base64Data.size());
    fwrite(base64Data.constData(), base64Data.size(), 1, file);
    fprintf(file, "\n\n");

    fclose(file);
    return true;
}

bool Rig::load(const QString & filename)
{
    FILE * file = fopen(filename.toLocal8Bit().constData(), "rb");
    if (!file) {
        qWarning() << "RIG Resource unavailable: " << filename << "\n";
        return false;
    }

    path = filename;

    animations.clear();
    bones.clear();
    currentAnimation = RIG_UNSELECTED;
    currentFrame     = RIG_UNSELECTED;
    playCursor       = 0.0f;
    playing          = false;

    while (true) {
        int c = fgetc(file);
        if (c == EOF) {
            break;

        }else if (c == '#') {
            skipLine(file);
            continue;

        }else if (c == '\t' || c == '\n' || c == '\r' || c == ' ') {
            continue;

        }else if (c == 'B') {
            Bone b{};
            fscanf(file, "%hu, %hu, %f, %f, %f, %f, %hu, %hx\n",
                &b.jointID1, &b.jointID2, &b.width, &b.length, &b.offset, &b.minWidth, &b.imageCount, &b.flags);
            if (b.imageCount > RIG_BONE_IMAGES_MAX) b.imageCount = RIG_BONE_IMAGES_MAX;
            bones.append(b);

        }else if (c == 'I') {
            uint16_t index = 0, imageID = 0;
            fscanf(file, "%hu, %hu\n", &index, &imageID);
            if (!bones.isEmpty() && index < RIG_BONE_IMAGES_MAX)
                bones.last().images[index] = imageID;

        }else if (c == 'A') {
            Animation a;
            a.name[0] = 0;
            fscanf(file, "%31s\n", a.name);
            a.name[RIG_NAME_MAX] = 0;
            animations.append(a);

        }else if (c == 'F') {
            int index = 0;
            fscanf(file, "%d\n", &index);
            if (!animations.isEmpty())
                animations.last().frames.append(Frame());

        }else if (c == 'J') {
            Joint j{};
            float x, y, z;
            fscanf(file, "%f, %f, %f, %hx\n", &x, &y, &z, &j.flags);
            j.pos  = QVector3D(x, y, z);
            j.apos = j.pos;
            if (!animations.isEmpty() && !animations.last().frames.isEmpty())
                animations.last().frames.last().joints.append(j);

        }else if (c == 'T') {
            uint16_t type = 0;
            uint64_t size = 0;
            fscanf(file, "%hu, %llx\n", &type, &size);
            if (type > 0 || size == 0 || size > TEXTURE_DATA_MAX) {
                skipLine(file);
                continue;
            }

            QByteArray base64Data((qsizetype) size, 0);
            if (fread(base64Data.data(), size, 1, file) != 1) {
                skipLine(file);
                continue;
            }
            skipLine(file);

            QByteArray imageData = QByteArray::fromBase64(base64Data);
            textures.loadFromData(imageData, "PNG");
        }
    }

    fclose(file);

    if (!animations.isEmpty())
        animationSelect(0);

    return true;
}
