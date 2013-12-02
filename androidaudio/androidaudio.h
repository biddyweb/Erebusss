// modified from https://groups.google.com/forum/?fromgroups=#!topic/android-qt/rpPa_W6PF1Y , by Adam Pigg
#pragma once

#include <QtGlobal> // need this to get Q_OS_ANDROID #define, which we need before we include anything else!

#if defined(Q_OS_ANDROID)

// for native audio
#include <jni.h>
#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>

class AndroidAudio {
    friend class AndroidSound;

    bool createEngine();
    void destroyEngine();
    bool startSoundPlayer();

    bool sound_ok;

    // engine interfaces
    SLObjectItf mEngineObject;
    SLEngineItf mEngineEngine;

    // output mix interfaces
    SLObjectItf mOutputMixObject;

public:
    explicit AndroidAudio();
    ~AndroidAudio();
};
#endif
