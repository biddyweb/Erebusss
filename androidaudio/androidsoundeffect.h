// modified from https://groups.google.com/forum/?fromgroups=#!topic/android-qt/rpPa_W6PF1Y , by Adam Pigg
#pragma once

#include <QtGlobal> // need this to get Q_OS_ANDROID #define, which we need before we include anything else!

#if defined(Q_OS_ANDROID)

#include <QDebug>
#include <QString>

#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>
#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>

class AndroidSoundEffect {
    char* mBuffer;
    int mLength;

public:
    explicit AndroidSoundEffect();
    ~AndroidSoundEffect();

    bool load(const char *path);
    bool unload();

    char *getBuffer() const {
        return this->mBuffer;
    }
    int getLength() const {
        return this->mLength;
    }
};
#endif
