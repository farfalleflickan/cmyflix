#pragma once
#include "conf.h"

typedef enum {
    SHOWS_MODE      = 1 << 0, // TV SHOWS MODE
    MOVIES_MODE     = 1 << 1, // MOVIES MODE
    DB_MODE         = 1 << 2, // DB MODE
    HTML_MODE       = 1 << 3, // HTML MODE
    CLN_MODE        = 1 << 4, // CLEAN MODE
    FIX_MODE        = 1 << 5, // FIX MODE
    FIX_ID_MODE     = 1 << 6, // FIX MODE ID
    FIX_POSTER_MODE = 1 << 7, // FIX MODE POSTER
    FIX_NAME_MODE   = 1 << 8, // FIX MODE NAME
    FIX_REFR_MODE   = 1 << 9  // FIX MODE REFRESH 
} progFlags;

void printHelp();
void printVersion();
void checkUpdate();
void *tvCode(void *args);
void *movieCode(void *args);
void cleanMode(progConfig *conf, progFlags flags);
