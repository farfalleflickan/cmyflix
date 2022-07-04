#pragma once
#include "conf.h"

typedef enum {
    NO_MODE=0 << 0, // NOOP?
    DI_MODE=1 << 0, // Find directories
    FI_MODE=1 << 1, // Find files
    MO_MODE=1 << 2, // Movie mode
    TV_MODE=1 << 3  // TV show mode
} ioMode;

struct fileList *find(progConfig *conf, char *dirPath, char *searchStr, ioMode mode, bool recursive);
