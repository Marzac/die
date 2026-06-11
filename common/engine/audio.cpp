/**
    DIE ENGINE
    Depth Integration Engine / A modern ray-caster
    (c) Fred's Lab 2024-2026
    Frédéric Meslin / info@fredslab.net
    SPDX-License-Identifier: MIT
    If used commercially, contributions, donations are highly appreciated.

    audio wrapper
*/

#include "audio.h"

#include <QUrl>
#include <QMediaDevices>
#include <QAudioDevice>
#include <QDebug>

Audio audio;

/*****************************************************************************/
Audio::Audio() :
    audioEngine(nullptr),
    audioListener(nullptr),
    soundDoorOpen(nullptr),
    soundDoorClose(nullptr),
    soundDoorLocked(nullptr)
{
}

void Audio::init()
{
    QAudioFormat format;
    format.setSampleRate(48000);
    format.setChannelCount(2);
    format.setSampleFormat(QAudioFormat::Int16);

    QAudioDevice info(QMediaDevices::defaultAudioOutput());
    if (!info.isFormatSupported(format)) {
        qWarning() << "Raw audio format not supported by backend, cannot play audio.";
        return;
    }

    audioEngine = new QAudioEngine(48000);
    audioEngine->setOutputMode(QAudioEngine::Stereo);
    audioEngine->setOutputDevice(info);
    audioEngine->setRoomEffectsEnabled(false);
    audioEngine->setMasterVolume(1.0f);
    audioEngine->setDistanceScale(QAudioEngine::DistanceScaleMeter / 8.0f);
    audioEngine->start();

    audioListener = new QAudioListener(audioEngine);
    audioListener->setPosition(QVector3D());
    audioListener->setRotation(QQuaternion());

    soundDoorOpen = soundLoad("qrc:/Sounds/map/door-open.wav");
    soundDoorClose = soundLoad("qrc:/Sounds/map/door-close.wav");
    soundDoorLocked = soundLoad("qrc:/Sounds/map/door-locked.wav");
}

void Audio::terminate()
{
    soundUnload(soundDoorOpen);
    soundUnload(soundDoorClose);
    soundUnload(soundDoorLocked);
    soundDoorOpen = nullptr;
    soundDoorClose = nullptr;
    soundDoorLocked = nullptr;

    delete audioListener;
    audioListener = nullptr;

    delete audioEngine;
    audioEngine = nullptr;
}

/*****************************************************************************/
QSpatialSound * Audio::soundLoad(const QString &resource)
{
    if (!audioEngine) return nullptr;

// Default source volume
    constexpr float defaultVolume = 2.0f;

    QSpatialSound * sound = new QSpatialSound(audioEngine);
    sound->setAutoPlay(false);
    sound->setSource(QUrl(resource));
    sound->setVolume(defaultVolume);
    sound->setPosition(QVector3D());
    return sound;
}

void Audio::soundUnload(QSpatialSound * sound)
{
    if (!sound) return;
    sound->stop();
    delete sound;
}
