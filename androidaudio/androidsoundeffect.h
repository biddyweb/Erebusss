// modified from https://groups.google.com/forum/?fromgroups=#!topic/android-qt/rpPa_W6PF1Y , by Adam Pigg
#pragma once

#include <QtGlobal> // need this to get Q_OS_ANDROID #define, which we need before we include anything else!

#if defined(Q_OS_ANDROID)

class AndroidSoundEffect {
    char *mBuffer;
    int mLength;

    bool load(const char *path);
    bool unload();

    AndroidSoundEffect();
public:
    ~AndroidSoundEffect();

    char *getBuffer() const {
        return this->mBuffer;
    }
    int getLength() const {
        return this->mLength;
    }

    static AndroidSoundEffect *create(const char *filename);
};
#endif
