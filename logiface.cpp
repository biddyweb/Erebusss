#include "logiface.h"
#include "game.h"

#include <cstdio>

void log(const char *text, ...) {
    va_list vlist;
    va_start(vlist, text);
    if( game_g != NULL ) {
        game_g->log(text, vlist);
    }
    va_end(vlist);
}
