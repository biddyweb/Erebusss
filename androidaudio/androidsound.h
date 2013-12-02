#pragma once

#include <QtGlobal> // need this to get Q_OS_ANDROID #define, which we need before we include anything else!

#if defined(Q_OS_ANDROID)

#include "androidaudio.h"
#include "androidsoundeffect.h"

// for native audio
#include <jni.h>
#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>

class AndroidSound {
    // buffer queue player interfaces
    SLObjectItf mPlayerObject;
    SLPlayItf mPlayerPlay;
    SLBufferQueueItf mPlayerQueue;

    // output volume interface - should ultimately be per sampl
    SLVolumeItf mPlayerVolume;

    // output seek interface
    //SLSeekItf mPlayerSeek;

    static AndroidAudio androidAudio;

    const AndroidSoundEffect *sound_effect;

    bool setSoundEffect(const AndroidSoundEffect *sound_effect);

    AndroidSound();
public:
    ~AndroidSound();

    void setVolume(int volume);
    void play(bool loop);

    static AndroidSound *create(const AndroidSoundEffect *sound_effect);
};

#endif
