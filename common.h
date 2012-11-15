#pragma once

#include <QtGlobal> // need this to get Q_OS_ANDROID #define, which we need before we include anything else!

#if defined(Q_OS_ANDROID) || defined(Q_OS_SYMBIAN) || defined(Q_WS_SIMULATOR) || defined(Q_WS_MAEMO_5)
const bool mobile_c = true;
#else
const bool mobile_c = false;
#endif

#ifdef Q_OS_ANDROID
#define WANT_ANDROID_SOUND // comment out to disable sound on Android (means can be built without OpenSLES)
#endif

#if defined(Q_OS_ANDROID)
#define DEPLOYMENT_PATH "assets:/"
#else
#define DEPLOYMENT_PATH ""
#endif

// remember to update version in PRO file!
const int versionMajor = 0;
const int versionMinor = 2;

enum Difficulty {
    DIFFICULTY_EASY = 0,
    DIFFICULTY_MEDIUM = 1,
    DIFFICULTY_HARD = 2,
    N_DIFFICULTIES = 3
};
