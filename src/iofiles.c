#include <dirent.h> 
#include <stdbool.h>
#include <stdio.h>
#include <bsd/string.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <errno.h>
#include "utils.h"
#include "fileList.h"
#include "iofiles.h"

// Own implementation with opendir/readdir. nftw might be faster? Dunno, nftw seemed slower in tests
struct fileList *find(progConfig *conf, char *dirPath, char *searchStr, ioMode mode, bool recursive) {
    DIR *directory = opendir(dirPath);
    if (directory == NULL) {
        printError("find warning", false, HYEL, "failed while opening directory \"%s\";\nError: %s;\n", dirPath, strerror(errno));
        return NULL;
    }
    fileList *list = NULL;
    for (struct dirent *fileP = readdir(directory); fileP != NULL; fileP = readdir(directory)) { // loop through all files in directory
        size_t nameLen = strlen(fileP->d_name);
        if (fileP->d_name[0] != '.' && fileP->d_name[nameLen - 1] != '~') { // ignore hidden files & such
            size_t pathLen = strlen(dirPath);
            char *path=NULL;
            mallocMacro(path, pathLen+nameLen+5, "find error");
            strlcpy(path, dirPath, pathLen+nameLen+5);
            if (dirPath[pathLen - 1] != '/') {
                strlcat(path, "/", pathLen+nameLen+5);
            }
            strlcat(path, fileP->d_name, pathLen+nameLen+5);
            struct stat statBuff;

            // d_type is not always true, thus stat is needed for S_ISDIR
            // fstat not used because path is needed regardless
            if (stat(path, &statBuff) != 0) {
                printError("find warning", false, HYEL, "failed running stat on \"%s\";\nError: %s;\n", path, strerror(errno));
                return NULL;
            }

            if (S_ISDIR(statBuff.st_mode) && (mode & DI_MODE)!=0) { // if file is directory and if mode has dir bit set
                if (recursive==true) {
                    if ((mode & FI_MODE)==0) { // if FI_MODE is *NOT* set
                        if (list==NULL) {
                            list=newList();
                        }
                        list = initList(list, NULL, 0);
                        addData(list, path);
                    }
                    fileList *temp = find(conf, path, searchStr, mode, recursive);
                    if (temp != NULL) {
                        list = joinLists(list, temp);
                    }
                } else {
                    fileList *newFile=newList();
                    addData(newFile, dirPath);
                    addData(newFile, fileP->d_name);
                    list=joinLists(newFile, list);
                }
            } else if (strstr(fileP->d_name, searchStr) && (mode & FI_MODE)!=0) { // if filename matches the string we looking for AND in file mode
                char **fileMatch = NULL;
                if ((mode & TV_MODE)!=0) { // if TV mode bit set
                    // if ( conf->tvDB_exists==true && (mode & CK_MODE)!=0 && strstr(conf->tvDB_str, path)!=NULL) { // if filename already in database & in 'check mode'
                    for (int i=0; i<conf->regexTVnum && fileMatch == NULL; i++) {
                        fileMatch=matchReg(path, conf->regexTVstr[i], conf->regexTVgroups);
                        if (fileMatch!=NULL) {
                            fileList *newFile=newList();
                            for (int j=1; j<=parseStrToInt(fileMatch[0]); j++) {
                                addData(newFile, fileMatch[j]);
                                if (j==1) {
                                    addData(newFile, dirPath);
                                    addData(newFile, fileP->d_name);
                                }
                            }
                            list = joinLists(newFile, list);                       
                        }
                    }
                } else if ((mode & MO_MODE)!=0) { // if movie mode bit set...
                    /*
                     * Nothing, else works just as well
                     */
                } else { // if in file mode but NOT in movie/tv mode...
                    fileList *newFile=newList();
                    addData(newFile, path);
                    addData(newFile, dirPath);
                    addData(newFile, fileP->d_name);
                    list=joinLists(newFile, list);
                }
                freeStrArr(fileMatch);
            }
            tryFree(path);
        }
    }
    closedir(directory);

    return list;
}
