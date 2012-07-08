#include "logiface.h"
#include "game.h"

#include <cstdio>

void log(const char *text, ...) {
    va_list vlist;
    char buffer[65536] = "";
    va_start(vlist, text);
    vsprintf(buffer,text,vlist);
    game_g->log(buffer);
}
