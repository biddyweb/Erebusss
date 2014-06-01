#pragma once

#include "common.h"

// lightweight file for logging, to avoid files having to include game.h just for logging

void log(const char *text, ...);

const bool LOGGING = true; // enable logging even for release builds, for now

#ifndef LOG
#define LOG if( !LOGGING ) ((void)0); else ::log
#endif

/** If test is not true:
  *   In debug versions, writes the test, file and line to the log, and exits (via assert).
  *   In release versions, we still write to the log file, but then continue.
  */
extern int n_assertion_failures;
#ifndef ASSERT_LOGGER
#ifdef _DEBUG

#define ASSERT_LOGGER(test) {                          \
        bool v = test;                                 \
        if( v )                                        \
                ((void)0);                             \
        else {                                         \
                n_assertion_failures++;                \
                LOG("ASSERTION FAILED:\n");            \
                LOG("%s\n", #test);                    \
                LOG("File: %s\n", __FILE__);           \
                LOG("Line: %d\n", __LINE__);           \
                assert(test);                          \
        }                                              \
}

#else

#define ASSERT_LOGGER(test) {                          \
        bool v = test;                                 \
        if( v )                                        \
                ((void)0);                             \
        else {                                         \
                n_assertion_failures++;                \
                LOG("ASSERTION FAILED:\n");            \
                LOG("%s\n", #test);                    \
                LOG("File: %s\n", __FILE__);           \
                LOG("Line: %d\n", __LINE__);           \
        }                                              \
}

#endif
#endif
