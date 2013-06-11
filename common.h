#pragma once

#include <QtGlobal> // need this to get Q_OS_ANDROID #define, which we need before we include anything else!

/** Platform #defines:
  * smallscreen_c: whether the platform has a smallscreen or not (phones, handhelds) - at some point, we may want to detect larger Android tablets as non-smallscreen.
  * touchscreen_c: whether to optimise the UI for a touchscreen rather than mouse/touchpad.
  * lightdistribution_c: whether the distribution misses large non-essential files (currently this means the intro music)
  */
#if defined(Q_OS_ANDROID) || defined(Q_OS_SYMBIAN) || defined(Q_WS_SIMULATOR) || defined(Q_WS_MAEMO_5)
const bool smallscreen_c = true;
const bool touchscreen_c = true;
const bool lightdistribution_c = true;
#else
const bool smallscreen_c = false;
const bool touchscreen_c = false;
const bool lightdistribution_c = false;
#endif

#if defined(Q_OS_ANDROID)
#define DEPLOYMENT_PATH "assets:/"
#else
#define DEPLOYMENT_PATH ""
#endif

// remember to update version in PRO file, and for Android/necessitas!
const int versionMajor = 0;
const int versionMinor = 10;

enum GameType {
    GAMETYPE_CAMPAIGN = 0,
    GAMETYPE_RANDOM = 1
};

enum Difficulty {
    DIFFICULTY_EASY = 0,
    DIFFICULTY_MEDIUM = 1,
    DIFFICULTY_HARD = 2,
    N_DIFFICULTIES = 3
};
