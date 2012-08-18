#pragma once

#if defined(Q_OS_ANDROID) || defined(Q_OS_SYMBIAN) || defined(Q_WS_SIMULATOR) || defined(Q_WS_MAEMO_5)
const bool mobile_c = true;
#else
const bool mobile_c = false;
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

enum Direction {
    DIRECTION_W = 0,
    DIRECTION_NW = 1,
    DIRECTION_N = 2,
    DIRECTION_NE = 3,
    DIRECTION_E = 4,
    DIRECTION_SE = 5,
    DIRECTION_S = 6,
    DIRECTION_SW = 7,
    N_DIRECTIONS = 8
};