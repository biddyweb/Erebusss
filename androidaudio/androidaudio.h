#ifndef ANDROIDAUDIO_H
#define ANDROIDAUDIO_H

// modified from https://groups.google.com/forum/?fromgroups=#!topic/android-qt/rpPa_W6PF1Y , by Adam Pigg

//#include <QtGlobal> // need this to get Q_OS_ANDROID #define, which we need before we include anything else!
#include "../common.h" // need this to get Q_OS_ANDROID #define, as well as WANT_ANDROID_SOUND

// n.b., need to faff around with #includes here, so it compiles on both Android and other platforms, even though we only need this file for Android

#include <QObject>

#if defined(Q_OS_ANDROID)
#ifdef WANT_ANDROID_SOUND

#include <QMap>

#include "androidsoundeffect.h"

// for native audio
#include <jni.h>
#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>
#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>

#endif
#endif

class AndroidAudio : public QObject
{
    Q_OBJECT
#if defined(Q_OS_ANDROID)
#ifdef WANT_ANDROID_SOUND
public:
    explicit AndroidAudio(QObject *parent = 0);
    ~AndroidAudio();

signals:

public slots:

    void registerSound(const QString& path, const QString &name);
    void playSound(const QString& name);
    void freeSound(const QString& name);

private:
    bool createEngine();
    void destroyEngine();
    bool startSoundPlayer();

    bool sound_ok;

    // engine interfaces
    SLObjectItf mEngineObject;
    SLEngineItf mEngineEngine;

    // output mix interfaces
    SLObjectItf mOutputMixObject;

    // buffer queue player interfaces - Effects
    SLObjectItf mPlayerObject;
    SLPlayItf mPlayerPlay;
    SLBufferQueueItf mPlayerQueue;

    QMap<QString, AndroidSoundEffect*> mSounds;
    int32_t mSoundCount;
#endif
#endif
};

#endif // ANDROIDAUDIO_H
