/**
    DIE ENGINE
    Depth Integration Engine / A modern ray-caster
    (c) Fred's Lab 2024-2026
    Frédéric Meslin / info@fredslab.net
    SPDX-License-Identifier: MIT
    If used commercially, contributions, donations are highly appreciated.

    rig model
*/

#include "rig.h"

#include <string.h>
#include <math.h>

/*****************************************************************************/
Rig::Rig()
{
    init();
}

/*****************************************************************************/
void Rig::init()
{
    clearAnimations();
    bones.clear();
    path.clear();
    textures = QImage();

    currentAnimation = RIG_UNSELECTED;
    currentFrame     = RIG_UNSELECTED;
    playCursor       = 0.0f;
    playing          = false;
}

void Rig::terminate()
{
    clearAnimations();
    bones.clear();
}

void Rig::clearAnimations()
{
    animations.clear();
    currentAnimation = RIG_UNSELECTED;
    currentFrame     = RIG_UNSELECTED;
    playCursor       = 0.0f;
    playing          = false;
}

/*****************************************************************************/
void Rig::update()
{
    Animation * a = currentAnimationPtr();
    if (!a || a->frames.isEmpty()) return;
    if (!playing) return;

    playCursor += 1.0f;
    if (playCursor >= a->frames.count())
        playCursor -= a->frames.count();

    currentFrame = (int)floorf(playCursor);
}

/*****************************************************************************/
int Rig::animationAdd(const char * name)
{
    Animation a;
    memset(&a.name, 0, sizeof(a.name));
    if (name) strncpy(a.name, name, RIG_NAME_MAX);
    a.frames.append(Frame());

    animations.append(a);
    currentAnimation = animations.count() - 1;
    currentFrame     = 0;
    playCursor       = 0.0f;
    return currentAnimation;
}

void Rig::animationDelete(int aId)
{
    if (aId < 0 || aId >= animations.count()) return;
    animations.removeAt(aId);

    if (animations.isEmpty()) {
        currentAnimation = RIG_UNSELECTED;
        currentFrame     = RIG_UNSELECTED;
    } else if (currentAnimation >= animations.count()) {
        animationSelect(animations.count() - 1);
    }
}

void Rig::animationSelect(int aId)
{
    if (aId < 0 || aId >= animations.count()) return;
    currentAnimation = aId;
    currentFrame     = animations[aId].frames.isEmpty() ? RIG_UNSELECTED : 0;
    playCursor       = 0.0f;
}

/*****************************************************************************/
int Rig::frameAdd()
{
    Animation * a = currentAnimationPtr();
    if (!a) return RIG_UNSELECTED;

    Frame f;
    if (currentFrame >= 0 && currentFrame < a->frames.count())
        f = a->frames[currentFrame];     // duplicate the current pose
    a->frames.append(f);

    currentFrame = a->frames.count() - 1;
    return currentFrame;
}

int Rig::frameInsert(int at)
{
    Animation * a = currentAnimationPtr();
    if (!a) return RIG_UNSELECTED;
    if (at < 0 || at > a->frames.count()) return RIG_UNSELECTED;

    Frame f;
    if (currentFrame >= 0 && currentFrame < a->frames.count())
        f = a->frames[currentFrame];     // duplicate the current pose
    a->frames.insert(at, f);

    currentFrame = at;
    return currentFrame;
}

void Rig::frameDelete(int fId)
{
    Animation * a = currentAnimationPtr();
    if (!a) return;
    if (fId < 0 || fId >= a->frames.count()) return;

    a->frames.removeAt(fId);

    if (a->frames.isEmpty()) {
        currentFrame = RIG_UNSELECTED;
    } else if (currentFrame >= a->frames.count()) {
        currentFrame = a->frames.count() - 1;
    }
    playCursor = (float)(currentFrame < 0 ? 0 : currentFrame);
}

void Rig::frameSelect(int fId)
{
    Animation * a = currentAnimationPtr();
    if (!a) return;
    if (fId < 0 || fId >= a->frames.count()) return;

    currentFrame = fId;
    playCursor   = (float)fId;
}

/*****************************************************************************/
void Rig::play()
{
    Animation * a = currentAnimationPtr();
    if (!a || a->frames.isEmpty()) return;
    playing = true;
}

void Rig::stop()
{
    playing = false;
}

/*****************************************************************************/
Animation * Rig::currentAnimationPtr()
{
    if (currentAnimation < 0 || currentAnimation >= animations.count())
        return nullptr;
    return &animations[currentAnimation];
}

Frame * Rig::currentFramePtr()
{
    Animation * a = currentAnimationPtr();
    if (!a) return nullptr;
    if (currentFrame < 0 || currentFrame >= a->frames.count())
        return nullptr;
    return &a->frames[currentFrame];
}
