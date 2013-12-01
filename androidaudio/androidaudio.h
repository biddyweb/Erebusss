// modified from https://groups.google.com/forum/?fromgroups=#!topic/android-qt/rpPa_W6PF1Y , by Adam Pigg
#pragma once

#include <QtGlobal> // need this to get Q_OS_ANDROID #define, which we need before we include anything else!

#if defined(Q_OS_ANDROID)

#include "androidsoundeffect.h"

// for native audio
#include <jni.h>
#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>
#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>

class AndroidAudio {
    bool createEngine();
    void destroyEngine();
    bool startSoundPlayer();

    bool sound_ok;

    // engine interfaces
    SLObjectItf mEngineObject;
    SLEngineItf mEngineEngine;

    // output mix interfaces
    SLObjectItf mOutputMixObject;

    // buffer queue player interfaces - should ultimately be per sample
    SLObjectItf mPlayerObject;
    SLPlayItf mPlayerPlay;
    SLBufferQueueItf mPlayerQueue;

    // output volume interface - should ultimately be per sample
    SLVolumeItf mPlayerVolume;

    // output seek interface - should ultimately be per sample
    //SLSeekItf mPlayerSeek;

public:
    explicit AndroidAudio();
    ~AndroidAudio();

    void setVolume(int volume);
    void playSound(const AndroidSoundEffect *sound, bool loop);
    AndroidSoundEffect *loadSound(const char *filename);


};
#endif
