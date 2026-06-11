/**
    DIE ENGINE
    Depth Integration Engine / A modern ray-caster
    (c) Fred's Lab 2024-2026
    Frédéric Meslin / info@fredslab.net
    SPDX-License-Identifier: MIT
    If used commercially, contributions, donations are highly appreciated.

    audio wrapper
*/

#ifndef AUDIO_H
#define AUDIO_H

#include <QAudioEngine>
#include <QAudioListener>
#include <QSpatialSound>

class Audio
{
public:
    Audio();

    /// \brief Initialise the audio engine and load the built-in sounds
    void init();

    /// \brief Unload the built-in sounds and shut the audio engine down
    void terminate();

    /**
        \brief Load a spatial sound from a resource path
        \param resource qrc or file URL of the sound
        \return new spatial sound, or nullptr when no audio engine is available
    */
    QSpatialSound * soundLoad(const QString &resource);

    /// \brief Stop and release a spatial sound
    void soundUnload(QSpatialSound * sound);

    QAudioEngine * audioEngine;
    QAudioListener * audioListener;

    QSpatialSound * soundDoorOpen;
    QSpatialSound * soundDoorClose;
    QSpatialSound * soundDoorLocked;
};

extern Audio audio;

#endif // AUDIO_H
