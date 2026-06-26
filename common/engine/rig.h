/**
    DIE ENGINE
    Depth Integration Engine / A modern ray-caster
    (c) Fred's Lab 2024-2026
    Frédéric Meslin / info@fredslab.net
    SPDX-License-Identifier: MIT
    If used commercially, contributions, donations are highly appreciated.

    rig model
*/

#ifndef RIG_H
#define RIG_H

#include "rigobjects.h"

#include <QList>
#include <QString>
#include <QImage>

#include <stdint.h>

static constexpr int RIG_NAME_MAX = 31;

/*****************************************************************************/
/**
    \brief A single pose: the joint positions for one frame

    Bones are not stored here; the skeleton topology lives on the Rig and is
    shared by every frame (bones reference joints by index).
*/
struct Frame {
    QList<Joint> joints;
};

/**
    \brief A named, ordered set of frames
*/
struct Animation {
    char name[RIG_NAME_MAX + 1];
    QList<Frame> frames;
};

/*****************************************************************************/
class Rig
{
public:
    Rig();

    void init();
    void terminate();

    /// \brief Advance the play cursor while playing, then refresh currentFrame
    void update();

    bool save(const QString & filename);
    bool load(const QString & filename);

// Animations
    int  animationAdd(const char * name);
    void animationDelete(int aId);
    void animationSelect(int aId);

// Frames, within the current animation
    int  frameAdd();            ///< append a copy of the current frame
    int  frameInsert(int at);   ///< insert a copy of the current frame at index
    void frameDelete(int fId);
    void frameSelect(int fId);

// Playback
    void play();
    void stop();

// Convenience access to the current animation / frame, nullptr when none
    Animation * currentAnimationPtr();
    Frame     * currentFramePtr();

    QString path;
    QImage  textures;

    QList<Bone> bones;          ///< skeleton topology, shared by every frame
    QList<Animation> animations;

    int   currentAnimation;
    int   currentFrame;
    float playCursor;
    bool  playing;

private:
    void clearAnimations();
};

#endif // RIG_H
