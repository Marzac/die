/**
    DIE ENGINE
    Depth Integration Engine / A modern ray-caster
    (c) Fred's Lab 2024-2026
    Frédéric Meslin / info@fredslab.net
    SPDX-License-Identifier: MIT
    If used commercially, contributions, donations are highly appreciated.

    tags / name-value pairs database
*/

#include "tags.h"

#include <string.h>
#include <stdio.h>

Tags tags;

/*****************************************************************************/
Tags::Tags()
{
    init();
}

void Tags::init()
{
    path.clear();
    clear();
}

void Tags::clear()
{
    tags.clear();
}

/*****************************************************************************/
bool Tags::load(const QString & filename)
{
    FILE * file = fopen(filename.toLocal8Bit().constData(), "rb");
    if (!file) return false;

    path = filename;

    while (true) {
        int c = fgetc(file);
        if (c == EOF) break;

        if (c == '#') {
            skipLine(file);
        } else if (c == '\t' || c == '\n' || c == '\r' || c == ' ') {
            continue;
        } else if (c == 'G') {
            Tag t;
            memset(&t, 0, sizeof(Tag));
            int n = fscanf(file, " %31[^,\n], %127[^\n]", t.name, t.value);
            if (n < 1) { skipLine(file); continue; }
            t.name[TAG_NAME_MAX] = t.value[TAG_VALUE_MAX] = 0;

            int id = findByName(t.name);
            if (id) {
                Tag & existing = tags[id - 1];
                memcpy(existing.value, t.value, sizeof(t.value));
            } else if (tags.count() < TAG_COUNT_MAX) {
                tags.append(t);
            }
        } else {
            skipLine(file);
        }
    }

    fclose(file);
    return true;
}

bool Tags::save(const QString & filename)
{
    FILE * file = fopen(filename.toLocal8Bit().constData(), "wb");
    if (!file) return false;

    path = filename;

    fprintf(file, "# == DIE TAGS ==\n");
    fprintf(file, "# Fred's Lab 2026\n");
    fprintf(file, "# 08.06.26 - V0.1\n");
    fprintf(file, "# == TAGS ==\n");
    fprintf(file, "# format: G name, value\n");
    for (Tag & t : tags)
        fprintf(file, "G %s, %s\n", t.name, t.value);

    fclose(file);
    return true;
}

/*****************************************************************************/
int Tags::findByName(const char * name) const
{
    for (int i = 0; i < tags.count(); i++) {
        if (strcmp(tags[i].name, name) == 0)
            return i + 1;
    }
    return 0;
}

int Tags::findOrAddByName(const char * name)
{
    if (!name[0] || strcmp(name, "None") == 0) return 0;

    int id = findByName(name);
    if (id) return id;
    if (tags.count() >= TAG_COUNT_MAX) return 0;

    Tag t;
    memset(&t, 0, sizeof(Tag));
    strncpy(t.name, name, TAG_NAME_MAX);
    t.name[TAG_NAME_MAX] = 0;
    tags.append(t);
    return tags.count();
}

/*****************************************************************************/
const char * Tags::nameForTag(uint16_t tag) const
{
    if (tag == 0 || (int)tag > tags.count()) return "None";
    return tags[tag - 1].name;
}

const char * Tags::valueForTag(uint16_t tag) const
{
    if (tag == 0 || (int)tag > tags.count()) return "";
    return tags[tag - 1].value;
}

/*****************************************************************************/
void Tags::skipLine(FILE * file)
{
    while (true) {
        int c = fgetc(file);
        if (c == EOF || c == '\n') return;
    }
}
