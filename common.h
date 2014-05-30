#pragma once

#include <QtGlobal> // need this to get Q_OS_ANDROID #define, which we need before we include anything else!

// the following are defined in main.cpp
extern bool smallscreen_c;
extern bool touchscreen_c;
extern bool lightdistribution_c;

extern char *DEPLOYMENT_PATH; // defined in main.cpp

// remember to update version in PRO file, and for Android/necessitas!
const int versionMajor = 0;
const int versionMinor = 14;

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
