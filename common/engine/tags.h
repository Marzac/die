/**
    DIE ENGINE
    Depth Integration Engine / A modern ray-caster
    (c) Fred's Lab 2024-2026
    Frédéric Meslin / info@fredslab.net
    SPDX-License-Identifier: MIT
    If used commercially, contributions, donations are highly appreciated.

    tags / name-value pairs database
*/

#ifndef TAGS_H
#define TAGS_H

#include <QList>
#include <QString>
#include <stdio.h>
#include <stdint.h>

static constexpr int TAG_NAME_MAX  = 31;
static constexpr int TAG_VALUE_MAX = 127;
static constexpr int TAG_COUNT_MAX = 65535;

/*****************************************************************************/
/**
    \brief A tag: name / value pair (null-terminated strings)
*/
typedef struct {
    char name[TAG_NAME_MAX + 1];
    char value[TAG_VALUE_MAX + 1];
} Tag;

/*****************************************************************************/
/**
    \brief Tags database
    \note tag ids are 1-based, id 0 means "None" / not found
*/
class Tags
{
public:
    Tags();

    /// \brief Clear the database and forget the current path
    void init();

    /// \brief Remove all the tags
    void clear();

    /**
        \brief Load tags from a file and merge them into the database
        \note tags with an already known name get their value updated
        \param filename path of the tags file
        \return true on success, false when the file cannot be opened
    */
    bool load(const QString & filename);

    /**
        \brief Save all the tags to a file
        \param filename path of the tags file
        \return true on success, false when the file cannot be opened
    */
    bool save(const QString & filename);

    /**
        \brief Find a tag by its name
        \return 1-based tag id, or 0 when not found
    */
    int findByName(const char * name) const;

    /**
        \brief Find a tag by its name, create it when missing
        \return 1-based tag id, or 0 for empty / "None" names and a full database
    */
    int findOrAddByName(const char * name);

    /// \brief Name of a tag, "None" for id 0 or out of range ids
    const char * nameForTag(uint16_t tag) const;

    /// \brief Value of a tag, "" for id 0 or out of range ids
    const char * valueForTag(uint16_t tag) const;

    QString path;
    QList<Tag> tags;

private:
    static void skipLine(FILE * file);
};

extern Tags tags;

#endif // TAGS_H
