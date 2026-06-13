/**
    DIE ENGINE
    Depth Integration Engine / A modern ray-caster
    (c) Fred's Lab 2024-2026
    Frédéric Meslin / info@fredslab.net
    SPDX-License-Identifier: MIT
    If used commercially, contributions, donations are highly appreciated.

    map load/save
*/

#include "map.h"

#include "audio.h"
#include "tags.h"

#include <QBuffer>
#include <QFileInfo>
#include <QDir>
#include <QUrl>
#include <QDebug>

#include <stdio.h>
#include <stdint.h>
#include <string.h>

// Maximum size of an embedded texture strip (base64 data)
static constexpr uint64_t TEXTURE_DATA_MAX = 16ull << 20;

/*****************************************************************************/
QString Map::getRelativePath(const QString & subMapPath)
{
    QFileInfo baseInfo(path);
    QDir baseDir = baseInfo.dir();
    QString relativePath = baseDir.relativeFilePath(subMapPath);
    return relativePath;
}

QString Map::resolveRelativePath(const QString & subMapPath)
{
    QFileInfo baseInfo(path);
    QDir baseDir = baseInfo.dir();
    QString absolutePath = baseDir.absoluteFilePath(subMapPath);
    return QDir::cleanPath(absolutePath);
}

/*****************************************************************************/
bool Map::save(const QString & filename, bool submap)
{
    FILE * file = fopen(filename.toLocal8Bit().constData(), "wb");
    if (!file) return false;

    fprintf(file, "# == DIE MAP == \n");
    fprintf(file, "# Fred's Lab 2026\n");
    fprintf(file, "# 08.06.26 - V0.5\n");

// ==== NODES ====
    fprintf(file, "# == NODES ==\n");
    fprintf(file, "# format: N x, y, z, metaA, metaB, metaC, tag, flags\n");
    for (const Node & n : nodes)
        fprintf(file, "N %+4.4f, %+4.4f, %+4.4f, %+4.4f, %+4.4f, %+4.4f, %.31s, %04hx\n", n.pos.x(), n.pos.y(), n.pos.z(), n.metaA, n.metaB, n.metaC, tags.nameForTag(n.tag), n.flags);
    fprintf(file, "\n");

// ==== WALLS ====
    fprintf(file, "# == WALLS ==\n");
    fprintf(file, "# format: W nodeID1, nodeID2, height, flags\n");
    fprintf(file, "# format: S surfaceID, textureID, scaleX, scaleY, shiftX, shiftY, flags\n");
    for (const Wall & w : walls) {
        fprintf(file, "W %04hu, %04hu, %+4.4f, %04hx\n", w.nodeID1, w.nodeID2, w.height, w.flags);
        for (int j = 0; j < WALL_SURFACES_COUNT; j++) {
            const Surface & s = w.surfaces[j];
            fprintf(file, "\tS %02d, %04hu, %+4.4f, %+4.4f, %+4.4f, %+4.4f, %04hx\n", j, s.id, s.scaleX, s.scaleY, s.shiftX, s.shiftY, s.flags);
        }
    } fprintf(file, "\n");

// ==== SUBMAPS ====
    fprintf(file, "# == SUBMAPS ==\n");
    fprintf(file, "# format: M nodeID, pan, scale, tag, flags, name, relative_path\n");
    for (const Submap & m : submaps) {
        const char * name = m.name[0] ? m.name : "None";
        const char * mapPath = m.path[0] ? m.path : "None";
        fprintf(file, "M %04hu, %+4.4f, %+4.4f, %.31s, %04hx, %.31s, %.127s\n", m.nodeID, m.pan, m.scale, tags.nameForTag(m.tag), m.flags, name, mapPath);
    } fprintf(file, "\n");

// ==== DOORS ====
    fprintf(file, "# == DOORS ==\n");
    fprintf(file, "# format: D nodeID, width, height, thick, mode, easing, start, stop, time, tag, flags, name\n");
    fprintf(file, "# format: S surfaceID, textureID, scaleX, scaleY, shiftX, shiftY, flags\n");
    for (const Door & d : doors) {
        const char * name = d.name[0] ? d.name : "None";
        fprintf(file, "D %04hu, %+4.4f, %+4.4f, %+4.4f, %02hu, %02hu, %+4.4f, %+4.4f, %+4.4f, %.31s, %04hx, %.31s\n", d.nodeID, d.width, d.height, d.thick, d.mode, d.easing, d.angle, d.swing, d.time, tags.nameForTag(d.tag), d.flags, name);
        for (int j = 0; j < DOOR_SURFACES_COUNT; j++) {
            const Surface & s = d.surfaces[j];
            fprintf(file, "\tS %02d, %04hu, %+4.4f, %+4.4f, %04hx\n", j, s.id, s.scaleX, s.scaleY, s.flags);
        }
    } fprintf(file, "\n");

// ==== LIFTS ====
    fprintf(file, "# == LIFTS ==\n");
    fprintf(file, "# format: E nodeID, width, length, thick, travel, time, mode, easing, tag, flags, name\n");
    fprintf(file, "# format: S surfaceID, textureID, scaleX, scaleY, flags\n");
    for (const Lift & e : lifts) {
        const char * name = e.name[0] ? e.name : "None";
        fprintf(file, "E %04hu, %+4.4f, %+4.4f, %+4.4f, %+4.4f, %+4.4f, %02hu, %02hu, %.31s, %04hx, %.31s\n", e.nodeID, e.width, e.length, e.thick, e.travel, e.time, e.mode, e.easing, tags.nameForTag(e.tag), e.flags, name);
        for (int j = 0; j < LIFT_SURFACES_COUNT; j++) {
            const Surface & s = e.surfaces[j];
            fprintf(file, "\tS %02d, %04hu, %+4.4f, %+4.4f, %04hx\n", j, s.id, s.scaleX, s.scaleY, s.flags);
        }
    } fprintf(file, "\n");

// ==== SPRITES ====
    fprintf(file, "# == SPRITES ==\n");
    fprintf(file, "# format: B nodeID, width, height, pan, tag, flags, name\n");
    fprintf(file, "# format: S surfaceID, bitmapID, scaleX, scaleY, shiftX, shiftY, flags\n");
    for (const Sprite & b : sprites) {
        const char * name = b.name[0] ? b.name : "None";
        fprintf(file, "B %04hu, %+4.4f, %+4.4f, %+4.4f, %.31s, %04hx, %.31s\n", b.nodeID, b.width, b.height, b.pan, tags.nameForTag(b.tag), b.flags, name);
        const Surface & s = b.surface;
        fprintf(file, "\tS %02d, %04hu, %+4.4f, %+4.4f, %+4.4f, %+4.4f, %04hx\n", 0, s.id, s.scaleX, s.scaleY, s.shiftX, s.shiftY, s.flags);
    } fprintf(file, "\n");

// ==== STAIRCASES ====
    fprintf(file, "# == STAIRCASES ==\n");
    fprintf(file, "# format: H nodeID, pan, height, width, length, steps, flags\n");
    fprintf(file, "# format: S surfaceID, textureID, scaleX, scaleY, shiftX, shiftY, flags\n");
    for (const Staircase & h : staircases) {
        fprintf(file, "H %04hu, %+4.4f, %+4.4f, %+4.4f, %+4.4f, %04hu, %04hx\n", h.nodeID, h.pan, h.height, h.width, h.length, h.steps, h.flags);
        for (int j = 0; j < STAIRCASE_SURFACES_COUNT; j++) {
            const Surface & s = h.surfaces[j];
            fprintf(file, "\tS %02d, %04hu, %+4.4f, %+4.4f, %04hx\n", j, s.id, s.scaleX, s.scaleY, s.flags);
        }
    } fprintf(file, "\n");

// ==== LIGHTS ====
    fprintf(file, "# == LIGHTS ==\n");
    fprintf(file, "# format: L nodeID, colorA, colorB, strength, speed, anim, tag, flags\n");
    for (const Light & l : lights) {
        fprintf(file, "L %04hu, %08x, %08x, %+4.4f, %+4.4f, %04hu, %.31s, %04hx\n", l.nodeID, l.colorA, l.colorB, l.strength, l.speed, l.anim, tags.nameForTag(l.tag), l.flags);
    } fprintf(file, "\n");

// ==== SPEAKERS ====
    fprintf(file, "# == SPEAKERS ==\n");
    fprintf(file, "# format: A nodeID, volume, size, pan, tag, flags, name, relative_path\n");
    for (const Speaker & a : speakers) {
        const char * name = a.name[0] ? a.name : "None";
        QByteArray relPath = a.path[0] ? getRelativePath(QString::fromUtf8(a.path)).toUtf8() : QByteArray("None");
        fprintf(file, "A %04hu, %+4.4f, %+4.4f, %+4.4f, %.31s, %04hx, %.31s, %.127s\n", a.nodeID, a.volume, a.size, a.pan, tags.nameForTag(a.tag), a.flags, name, relPath.constData());
    }
    fprintf(file, "\n");

// ==== PATHS ====
    fprintf(file, "# == PATHS ==\n");
    fprintf(file, "# format: P nodesCount, pan, tag, flags, name\n");
    fprintf(file, "# format: Q index, nodeID\n");
    for (const Path & p : paths) {
        const char * name = p.name[0] ? p.name : "None";
        fprintf(file, "P %02hu, %+4.4f, %.31s, %04hx, %.31s\n", p.nodesCount, p.pan, tags.nameForTag(p.tag), p.flags, name);
        for (int j = 0; j < p.nodesCount; j++)
            fprintf(file, "\tQ %02d, %04hu\n", j, p.nodes[j]);
    } fprintf(file, "\n");

    if (!submap) {
// ==== ENVIRONMENT ====
        fprintf(file, "# == ENVIRONMENT ==\n");
        fprintf(file, "# format: R ambientColor, ambientStrength, rayColor, rayStrength, hour, angle\n");
        fprintf(file, "R %06x, %+4.4f, %06x, %+4.4f, %+4.4f, %+4.4f\n",
            sun.ambientColor & 0xFFFFFF, sun.ambientStrength,
            sun.rayColor & 0xFFFFFF, sun.rayStrength, sun.hour, sun.angle);
        fprintf(file, "# format: F color, distanceNear, distanceFar, flags\n");
        fprintf(file, "F %06x, %+4.4f, %+4.4f, %04hx\n",
            fog.color & 0xFFFFFF, fog.distanceNear, fog.distanceFar, fog.flags);
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
    }

    fclose(file);
    return true;
}

bool Map::load(const QString & filename)
{
    FILE * file = fopen(filename.toLocal8Bit().constData(), "rb");
    if (!file) {
        qWarning() << "MAP Resource unavailable: " << filename << "\n";
        return false;
    }

    path = filename;
    origin = QVector3D(0.0f, 0.0f, 0.0f);
    scale = 1.0f;
    pan = 0.0f;

    clearObjects();
    resetEnvironment();

    char lastC = ' ';
    char tagStr[32];

    while (true) {
        int c = fgetc(file);
        if (c == EOF) {
            break;

        }else if (c == '#') {
            skipLine(file);
            continue;

        }else if (c == '\t' || c == '\n') {
            continue;

        }else if (c == 'N') {
            lastC = 'N';
            Node n{};
            float x, y, z;
            tagStr[0] = 0;
            fscanf(file, "%f, %f, %f, %f, %f, %f, %31[^,], %hx\n", &x, &y, &z, &n.metaA, &n.metaB, &n.metaC, tagStr, &n.flags);
            n.tag = tags.findOrAddByName(tagStr);
            n.pos = QVector3D(x, y, z);
            nodes.append(n);

        }else if (c == 'W') {
            lastC = 'W';
            Wall w{};
            fscanf(file, "%hu, %hu, %f, %hx\n", &w.nodeID1, &w.nodeID2, &w.height, &w.flags);
            if (w.nodeID1 >= nodes.count()) { lastC = ' '; continue; }
            if (w.nodeID2 >= nodes.count()) { lastC = ' '; continue; }
            if (w.nodeID1 == w.nodeID2)     { lastC = ' '; continue; }
            walls.append(w);

        }else if (c == 'M') {
            lastC = 'M';
            Submap m;
            memset(&m, 0, sizeof(Submap));
            tagStr[0] = 0;
            fscanf(file, "%hu, %f, %f, %31[^,], %hx, ", &m.nodeID, &m.pan, &m.scale, tagStr, &m.flags);
            m.tag = tags.findOrAddByName(tagStr);
            fscanf(file, "%31[^,], %127s\n", m.name, m.path);
            m.name[OBJECT_NAME_MAX] = m.path[OBJECT_PATH_MAX] = 0;
            if (m.nodeID >= nodes.count()) { lastC = ' '; continue; }
            submaps.append(m);

        }else if (c == 'D') {
            lastC = 'D';
            Door d;
            memset(&d, 0, sizeof(Door));
            tagStr[0] = 0;
            fscanf(file, "%hu, %f, %f, %f, %hu, %hu, %f, %f, %f, %31[^,], %hx, ", &d.nodeID, &d.width, &d.height, &d.thick, &d.mode, &d.easing, &d.angle, &d.swing, &d.time, tagStr, &d.flags);
            d.tag = tags.findOrAddByName(tagStr);
            fscanf(file, "%31s\n", d.name);
            d.name[OBJECT_NAME_MAX] = 0;
            if (d.nodeID >= nodes.count()) { lastC = ' '; continue; }
            doors.append(d);

        }else if (c == 'E') {
            lastC = 'E';
            Lift e;
            memset(&e, 0, sizeof(Lift));
            tagStr[0] = 0;
            fscanf(file, "%hu, %f, %f, %f, %f, %f, %hu, %hu, %31[^,], %hx, ", &e.nodeID, &e.width, &e.length, &e.thick, &e.travel, &e.time, &e.mode, &e.easing, tagStr, &e.flags);
            e.tag = tags.findOrAddByName(tagStr);
            fscanf(file, "%31s\n", e.name);
            e.name[OBJECT_NAME_MAX] = 0;
            if (e.nodeID >= nodes.count()) { lastC = ' '; continue; }
            lifts.append(e);

        }else if (c == 'B') {
            lastC = 'B';
            Sprite b;
            memset(&b, 0, sizeof(Sprite));
            tagStr[0] = 0;
            fscanf(file, "%hu, %f, %f, %f, %31[^,], %hx, ", &b.nodeID, &b.width, &b.height, &b.pan, tagStr, &b.flags);
            b.tag = tags.findOrAddByName(tagStr);
            fscanf(file, "%31s\n", b.name);
            b.name[OBJECT_NAME_MAX] = 0;
            if (b.nodeID >= nodes.count()) { lastC = ' '; continue; }
            sprites.append(b);

        }else if (c == 'H') {
            lastC = 'H';
            Staircase h;
            memset(&h, 0, sizeof(Staircase));
            fscanf(file, "%hu, %f, %f, %f, %f, %hu, %hx", &h.nodeID, &h.pan, &h.height, &h.width, &h.length, &h.steps, &h.flags);
            if (h.nodeID >= nodes.count()) { lastC = ' '; continue; }
            staircases.append(h);

        }else if (c == 'L') {
            lastC = 'L';
            Light l;
            memset(&l, 0, sizeof(Light));
            tagStr[0] = 0;
            fscanf(file, "%hu, %x, %x, %f, %f, %hu, %31[^,], %hx", &l.nodeID, &l.colorA, &l.colorB, &l.strength, &l.speed, &l.anim, tagStr, &l.flags);
            l.tag = tags.findOrAddByName(tagStr);
            if (l.nodeID >= nodes.count()) { lastC = ' '; continue; }
            lights.append(l);

        }else if (c == 'A') {
            lastC = 'A';
            Speaker a;
            memset(&a, 0, sizeof(Speaker));
            tagStr[0] = 0;
            fscanf(file, "%hu, %f, %f, %f, %31[^,], %hx, ", &a.nodeID, &a.volume, &a.size, &a.pan, tagStr, &a.flags);
            a.tag = tags.findOrAddByName(tagStr);
            fscanf(file, "%31[^,], %127s\n", a.name, a.path);
            a.name[OBJECT_NAME_MAX] = a.path[OBJECT_PATH_MAX] = 0;
            if (a.nodeID >= nodes.count()) { lastC = ' '; continue; }
            speakers.append(a);

        }else if (c == 'P') {
            lastC = 'P';
            Path p;
            memset(&p, 0, sizeof(Path));
            tagStr[0] = 0;
            fscanf(file, "%hu, %f, %31[^,], %hx, ", &p.nodesCount, &p.pan, tagStr, &p.flags);
            p.tag = tags.findOrAddByName(tagStr);
            fscanf(file, "%31s\n", p.name);
            p.name[OBJECT_NAME_MAX] = 0;
            if (p.nodesCount > PATH_NODES_MAX) p.nodesCount = PATH_NODES_MAX;
            paths.append(p);

        }else if (c == 'Q') {
            uint16_t index = 0, nodeID = 0;
            fscanf(file, "%hu, %hu\n", &index, &nodeID);
            if (lastC == 'P' && !paths.isEmpty() &&
                index < PATH_NODES_MAX && nodeID < nodes.count())
                paths.last().nodes[index] = nodeID;

        }else if (c == 'S') {
            Surface s;
            memset(&s, 0, sizeof(Surface));
            uint16_t type = 0;
            fscanf(file, "%hu,", &type);
            fscanf(file, "%hu, %f, %f, %hx\n", &s.id, &s.scaleX, &s.scaleY, &s.flags);

            if (lastC == 'W') {
                if (type >= WALL_SURFACES_COUNT || walls.isEmpty()) continue;
                walls.last().surfaces[type] = s;

            } else if (lastC == 'D') {
                if (type >= DOOR_SURFACES_COUNT || doors.isEmpty()) continue;
                doors.last().surfaces[type] = s;

            } else if (lastC == 'E') {
                if (type >= LIFT_SURFACES_COUNT || lifts.isEmpty()) continue;
                lifts.last().surfaces[type] = s;

            }else if (lastC == 'B') {
                if (type > 0 || sprites.isEmpty()) continue;
                sprites.last().surface = s;

            } else if (lastC == 'H') {
                if (type >= STAIRCASE_SURFACES_COUNT || staircases.isEmpty()) continue;
                staircases.last().surfaces[type] = s;
            }

        }else if (c == 'R') {
            fscanf(file, "%x, %f, %x, %f, %f, %f",
                &sun.ambientColor, &sun.ambientStrength,
                &sun.rayColor, &sun.rayStrength, &sun.hour, &sun.angle);

        }else if (c == 'F') {
            fscanf(file, "%x, %f, %f, %hx",
                &fog.color, &fog.distanceNear, &fog.distanceFar, &fog.flags);

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

    bool result = true;
    result &= loadSubmaps();
    result &= loadSounds();

    computeAllWalls();
    updateAllDoors();

    return result;
}

/*****************************************************************************/
bool Map::loadSubmaps()
{
    bool result = true;
    maps.clear();

    for (Submap & m : submaps) {
        maps.append(Map());
        QString relPath = QString::fromUtf8(m.path);
        QString absPath = resolveRelativePath(relPath);

        Map & subMap = maps[maps.count() - 1];
        subMap.path = relPath;
        result &= subMap.load(absPath);
    }

    return result;
}

bool Map::loadSounds()
{
    bool result = true;
    for (QSpatialSound * s : sounds)
        audio.soundUnload(s);
    sounds.clear();

    for (Speaker & a : speakers) {
        QString absPath = resolveRelativePath(QString::fromUtf8(a.path));
        QSpatialSound * s = audio.soundLoad(QUrl::fromLocalFile(absPath).toString());
        if (!s) result = false;
        sounds.append(s);
        speakerApplyFlags(sounds.count() - 1);
    }

    return result;
}

/*****************************************************************************/
void Map::skipLine(FILE * file)
{
    while (true) {
        int c = fgetc(file);
        if (c == EOF) return;
        if (c == '\n') return;
    }
}
