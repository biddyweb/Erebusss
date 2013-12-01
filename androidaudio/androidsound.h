#pragma once

#include <QtGlobal> // need this to get Q_OS_ANDROID #define, which we need before we include anything else!

#if defined(Q_OS_ANDROID)

class AndroidAudio;
class AndroidSoundEffect;

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

    const AndroidSoundEffect *sound_effect;

public:
    AndroidSound();
    ~AndroidSound();

    bool setSoundEffect(const AndroidAudio *android_audio, const AndroidSoundEffect *sound_effect);
    void setVolume(int volume);
    void playSound(bool loop);
};

#endif
