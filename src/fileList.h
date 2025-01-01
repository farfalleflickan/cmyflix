#pragma once
#include <cjson/cJSON.h>
#include "iofiles.h"

typedef struct fileList {
    struct fileList *next;
    char **data;
    size_t dataSize;
    size_t listSize;
} fileList;

typedef enum {
    INTEGER=0,
    STRING=1
} sortMode;

struct fileList *newList();
fileList *initList(fileList *next, char **data, size_t dSize);
fileList *add(fileList *head, char **data, size_t dSize);
fileList *joinLists(fileList *file, fileList *toAdd);
fileList *sortedJoinLists(fileList *part1, fileList *part2, size_t cmpPos, bool isAscending, sortMode mode);
fileList *JSONtofileList(cJSON *json, ioMode mode);
void splitList(fileList *list, fileList **part1, fileList **part2);
// mergeSort a list
void msortList(fileList **list, size_t cmpPos, bool isAscending, sortMode mode);
fileList *reverseList(fileList *head);
void freeList(struct fileList *list);
void printList(fileList *list);
// save fileList to a file, header is a string to be put at the top of the file
// footer is to be put at the end of file
void fileListToFile(fileList *list, char *dest, char *headerStr, char *footerStr);
char *fileListToJSONStr(fileList *list);
// append data to list
void addData(fileList *list, char *data);
