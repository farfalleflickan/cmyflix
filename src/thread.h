#pragma once
#include <cjson/cJSON.h>
#include "conf.h"
#include "fileList.h"
#include "main.h"

typedef struct threadStruct{
    fileList *list;
    progConfig *conf;
    progFlags flags;
    char **data;
    unsigned int id;
    cJSON *oldJSON;
} threadStruct;
