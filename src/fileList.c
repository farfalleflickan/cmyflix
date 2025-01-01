#include <stdlib.h>
#include <stdbool.h>
#include <bsd/string.h>
#include <errno.h>
#include <stdarg.h>
#include "fileList.h"
#include "utils.h"

// create empty fileList 
struct fileList *newList() {
    fileList *list = NULL;
    mallocMacro(list, sizeof(struct fileList), "newList error");
    list->next = NULL;
    list->data = NULL;
    list->dataSize = 0;
    list->listSize = 1;
    return list;
}

// initialize fileList
fileList *initList(fileList *next, char **data, size_t dSize) {
    fileList *list = newList();
    list->data = data;
    list->next = next;
    list->dataSize = dSize;
    list->listSize = 1;
    return list;
}

// add link to filelist
fileList *add(fileList *head, char **data, size_t dSize) {
    if (head!=NULL) {
        fileList *list = initList(head, data, dSize);
        list->listSize=head->listSize;
        head = list;
        head->listSize++;
    } else {
        head=initList(NULL, data, dSize);
    }
    return head;
}

// append 2 list to each other
fileList *joinLists(fileList *file, fileList *toAdd) {
    fileList *cursor = file;
    if (file == NULL && toAdd == NULL) { // both list NULL, return NULL
        return NULL;
    } else if (file != NULL && toAdd == NULL) { // toAdd is NULL, return file
        return file;
    } else if (file == NULL && toAdd != NULL) { // file is NULL, return toAdd
        return toAdd;
    } else { 
        // loop through file to append toAdd at the end
        for (size_t i=0; cursor->next != NULL; cursor = cursor->next,i++) {
        }
        cursor->next = toAdd;
        file->listSize+=toAdd->listSize;
        return file;
    }
}

// split list in half, helper for mergesort
// every other element goes to part2, rest goes to part 1
void splitList(fileList *list, fileList **part1, fileList **part2) {
    if (list == NULL || list->next == NULL) {
        *part1=list;
        *part2=NULL;
        return;
    }
    // temporary pointers
    fileList *fast=list->next;
    fileList *slow=list;
    while(fast!=NULL) {
        fast = fast->next;
        if (fast != NULL) {
            slow = slow->next;
            fast = fast->next;
        }
    }
    *part1=list;
    *part2=slow->next;
    slow->next=NULL;
}

// join 2 lists in a sorted manner (ascending or descending), depending on sortMode, helper for mergesort
// cmpPos specifies the position of the data to compare for sorting
// mode is a sortMode enum, is either INTEGER or STRING
fileList *sortedJoinLists(fileList *part1, fileList *part2, size_t cmpPos, bool isAscending, sortMode mode) {
    if (part1==NULL) {
        return part2;
    } else if (part2==NULL) {
        return part1;
    }
    fileList *list=NULL;
    if (mode==INTEGER) {
        int data1=0, data2=0;

        if (part1->dataSize>cmpPos && part2->dataSize>cmpPos) {
            data1=parseStrToInt(part1->data[cmpPos]);
            data2=parseStrToInt(part2->data[cmpPos]);
        }

        if (data1 <= data2 && isAscending==true ) { 
            list=part1;
            list->next=sortedJoinLists(part1->next, part2, cmpPos, isAscending, mode);
        } else {
            list=part2;
            list->next=sortedJoinLists(part1, part2->next, cmpPos, isAscending, mode);
        }
    } else if (mode==STRING) {
       char *name1=NULL, *name2=NULL;
       if (cmpPos < part1->dataSize && part1->data[cmpPos]!=NULL) {
           name1=part1->data[cmpPos];
       }
       if (cmpPos < part2->dataSize && part2->data[cmpPos]!=NULL) {
           name2=part2->data[cmpPos];
       }

        if (name1 != NULL && name2 != NULL) {
            int cmp = strcasecmp(name1, name2);
            if ((cmp < 0 && isAscending) || (cmp > 0 && !isAscending)) {
                list = part1;
                list->next = sortedJoinLists(part1->next, part2, cmpPos, isAscending, mode);
            } else {
                list = part2;
                list->next = sortedJoinLists(part1, part2->next, cmpPos, isAscending, mode);
            }
        } else if (name1 == NULL) {
            list = part2;
            list->next = sortedJoinLists(part1, part2->next, cmpPos, isAscending, mode);
        } else if (name2 == NULL) {
            list = part1;
            list->next = sortedJoinLists(part1->next, part2, cmpPos, isAscending, mode);
        }
    }
    return list;
}

// mergesort list
// cmpPos is the position of the data to compare for sortedJoinList
void msortList(fileList **list, size_t cmpPos, bool isAscending, sortMode mode) {
    if ((*list)!=NULL && (*list)->next!=NULL) {
        int initialSize=(*list)->listSize;
        fileList *part1, *part2;
        splitList(*list, &part1, &part2);
        // recursively split the list
        msortList(&part1, cmpPos, isAscending, mode);
        msortList(&part2, cmpPos, isAscending, mode);

        *list=sortedJoinLists(part1, part2, cmpPos, isAscending, mode);
        // fix listSize
        (*list)->listSize=initialSize;
    }
}

// print list content to stdout, debug use only
void printList(fileList *list) {
    if (list!=NULL) {
        size_t j=0;
        printf("List start, listSize:\t%zu\n", list->listSize);
        for (fileList *temp=list; temp!=NULL; temp=temp->next) {
            printf("\tdataSize:\t%zu;\n", temp->dataSize);
            if (temp->dataSize==0) {
                printf("\tlist[%zu]->data[]:\tNO DATA!;\n", j);
            } else {
                for (size_t i=0; i<temp->dataSize; i++) {
                    printf("\tlist[%zu]->data[%zu]:\t%s;\n", j, i, temp->data[i]);
                }
            }
            j++;
        }
        printf("List end\n\n");
    } else {
        fatalError_abort("printList error", "cannot print list, it is NULL!\n");
    }
}

// free all contents of filelist
void freeList(struct fileList *list) {
    for (fileList *temp=list; temp!=NULL; ) {
        fileList *tempNext=temp->next;
        for (size_t i = 0; i < temp->dataSize; i++) {
            tryFree(temp->data[i]);
        }

        tryFree(temp->data);
        tryFree(temp);
        temp=tempNext;
    }
    list=NULL;
}

// append data to list
void addData(fileList *list, char *data) {
    size_t size = 0;
    if (list->dataSize == 0) {
        list->dataSize = 1;
        size = 1;
    } else {
        size = list->dataSize + 1;
        list->dataSize = size;
    }
    list->data = realloc(list->data, (size * sizeof (char **)));
    if (list->data != NULL) {
        size_t strLen = strlen(data)+1;

        list->data[size-1] = NULL;
        mallocMacro(list->data[size-1], strLen, "addData error");
        strlcpy(list->data[size - 1], data, strLen);
    } else {
        fatalError_abort("addData error", "realloc failed;\nError: %s;\n", strerror(errno));
    }
}

// convert JSON to fileList
// mode specifies the type of JSON, ie: TVSHOW or MOVIE (note: only MO_MODE, MOVIE, has different behaviour)
fileList *JSONtofileList(cJSON *json, ioMode mode) {
    fileList *list = NULL;
    cJSON *item=NULL;
    cJSON_ArrayForEach(item, json) {
        char *tempStr=cJSON_Print(item);
        list=add(list, NULL, 0);
        addData(list, tempStr);
        if (mode==MO_MODE) {
            char *movieName=cJSON_GetStringValue(cJSON_GetObjectItem(item, "Movie"));
            addData(list, movieName);
        }
        tryFree(tempStr);
    }
    return list;
}

// save fileList to a file, takes a header and a footer to before and after the list data content
void fileListToFile(fileList *list, char *dest, char *headerStr, char *footerStr) {
    FILE *file=fopen(dest, "w+");
    if (file!=NULL) {
        if (headerStr!=NULL && fputs(headerStr, file)==EOF) {
            fatalError_abort("fileListToFile error", "failed while writing to file \"%s\";\nError: %s;\n", dest, strerror(errno));
        }
        for (fileList *temp=list; temp!=NULL; temp=temp->next) {
            for (size_t i=0; i<temp->dataSize; i++) {
                if (fputs(temp->data[i], file)==EOF) {
                    fatalError_abort("fileListToFile error", "failed while writing to file \"%s\";\nError: %s;\n", dest, strerror(errno));
                }
            }
            if (temp->next!=NULL) {
                if (fputs(",\n", file)==EOF) {
                    fatalError_abort("fileListToFile error", "failed while writing to file \"%s\";\nError: %s;\n", dest, strerror(errno));
                }
            } else if (fputs("\n", file)==EOF) {
                fatalError_abort("fileListToFile error", "failed while writing to file \"%s\";\nError: %s;\n", dest, strerror(errno));
            }
        }
        if (footerStr!=NULL && fputs(footerStr, file)==EOF) {
            fatalError_abort("fileListToFile error", "failed while writing to file \"%s\";\nError: %s;\n", dest, strerror(errno));
        }
        if (fputs("\0", file)==EOF) {
            fatalError_abort("fileListToFile error", "failed while writing to file \"%s\";\nError: %s;\n", dest, strerror(errno));
        }
    } else {
            fatalError_abort("fileListToFile error", "failed while writing to file \"%s\";\nError: %s;\n", dest, strerror(errno));
    }
    printInfo("fileListToFile", false, "done saving to \"%s\";\n", dest);
    fclose(file);
}

// fileList to single string
char *fileListToJSONStr(fileList *list) {
    size_t sizeStr=4;
    char *listStr=NULL;
    mallocMacro(listStr, sizeStr, "fileListToChar error");
    listStr[0]='\0';
    snprintf(listStr, sizeStr, "[\n");

    for (fileList *temp=list; temp!=NULL; temp=temp->next) {
        for (size_t i=0; i<temp->dataSize; i++) {
            sizeStr+=strlen(temp->data[i]);
            listStr=realloc(listStr, sizeStr);
            if (listStr==NULL) {
                fatalError_abort("fileListToChar", "could not realloc listStr;\nError: %s;\n", strerror(errno));
            }
            strlcat(listStr, temp->data[i], sizeStr);
        }
        if (temp->next!=NULL) {
            sizeStr+=3;
            listStr=realloc(listStr, sizeStr);
            if (listStr==NULL) {
                fatalError_abort("fileListToChar", "could not realloc listStr;\nError: %s;\n", strerror(errno));
            }
            strlcat(listStr, ",\n", sizeStr);
        } else {
            sizeStr+=2;
            listStr=realloc(listStr, sizeStr);
            if (listStr==NULL) {
                fatalError_abort("fileListToChar", "could not realloc listStr;\nError: %s;\n", strerror(errno));
            }
            strlcat(listStr, "\n", sizeStr);
        }
    }
    sizeStr+=2;
    listStr=realloc(listStr, sizeStr);
    if (listStr==NULL) {
        fatalError_abort("fileListToChar", "could not realloc listStr;\nError: %s;\n", strerror(errno));
    }
    strlcat(listStr, "]", sizeStr);
    return listStr;
}

fileList *reverseList(fileList *head) {
    fileList *prev = NULL;
    fileList *current = head;
    fileList *next = NULL;
    size_t listSize = 0;
    
    while (current != NULL) {
        next = current->next;    
        current->next = prev;    
        prev = current;
        current->listSize = ++listSize;
        current = next;          
    }

    return prev; // The new head of the reversed list
}