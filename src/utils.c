#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <stdarg.h>
#include <bsd/string.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include <regex.h>
#include <ctype.h>
#include <libgen.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <libgen.h>
#include "movies.h"
#include "tvshow.h"
#include "utils.h"
#include "iofiles.h"
#include "fileList.h"
#include "cwalk.h"

extern unsigned int PRINT_MODE; 
/* PRINT_MODE: 
   0=default mode, (warnings, minimal progress messages)
   1=verbose mode (add function in error/warnings, additional progress messages), 
   2=quiet mode (print only warnings/errors (no progress messages)), 
   3=quieter mode (print only errors (no progress messages)), 
   4=quietest mode (print nothing) */
extern FILE *LOGFILE;                                 
extern pthread_mutex_t *mutex_stdout;
extern struct timeval timeProgStart;

// reset colors to defaults for stdout and stderr
void resetSTDColors() {
    if (LOGFILE==NULL) {
        while (pthread_mutex_trylock(mutex_stdout)!=0) {
            msleep(10);
        }
        fprintf(stdout, COLOR_RESET);
        fprintf(stderr, COLOR_RESET);
        pthread_mutex_unlock(mutex_stdout);
    }
}

void printInfo(const char *caller, bool extraInfo, char *why, ...) {
    while (pthread_mutex_trylock(mutex_stdout)!=0) {
        msleep(10);
    }
    FILE *output_ptr=stdout;
    if (LOGFILE!=NULL) {
        output_ptr=LOGFILE;
    }
    va_list arg;
    va_start(arg, why);
    if (extraInfo==true && PRINT_MODE==1) {
        if (strlen(caller)>0) {
            fprintf(output_ptr, "%s [%Lf] %s%s: ", COLOR_RESET, getElapsedTime(), HBLU, caller);
        }
        vfprintf(output_ptr, why, arg);
        fprintf(output_ptr, COLOR_RESET);
    } else if (extraInfo==false) {
        if (strlen(caller)>0 && PRINT_MODE==1) {
            fprintf(output_ptr, "%s [%Lf] %s%s: ", COLOR_RESET, getElapsedTime(), HGRN, caller);
            vfprintf(output_ptr, why, arg);
        } else if (PRINT_MODE<2){
            char *whyCopy=NULL;
            size_t whySize=strlen(why)+1;
            mallocMacro(whyCopy, whySize+1, "printInfo error");
            strlcpy(whyCopy, why, whySize);
            whyCopy[0]=toupper(why[0]);
            fprintf(output_ptr, HGRN);
            vfprintf(output_ptr, whyCopy, arg);
            tryFree(whyCopy);
        }
    }
    fprintf(output_ptr, COLOR_RESET);
    va_end(arg);
    pthread_mutex_unlock(mutex_stdout);
}

void printError(const char *caller, bool extraInfo, const char *colorStr, char *why, ...) {
    if ((extraInfo==true && PRINT_MODE==1) || extraInfo==false) {
        va_list arg;
        va_start(arg, why);
        vprintError(caller, colorStr, why, arg);
        va_end(arg);
    }
}

void vprintError(const char *caller, const char *colorStr, char *why, va_list arg) {
    while (pthread_mutex_trylock(mutex_stdout)!=0) {
        msleep(10);
    }
    FILE *output_ptr=stderr;
    if (LOGFILE!=NULL) {
        output_ptr=LOGFILE;
    }
    if (strlen(caller)>0 && PRINT_MODE==1) {
        fprintf(output_ptr, "%s [%Lf] %s%s: ", COLOR_RESET, getElapsedTime(), colorStr, caller);
        vfprintf(output_ptr, why, arg);
    } else {
        if (strlen(caller)>0 && (PRINT_MODE<3 || (PRINT_MODE==3 && strstr(caller, "warning")==NULL))) {
            char *whyCopy=NULL;
            size_t whySize=strlen(why)+1;
            mallocMacro(whyCopy, whySize, "vprintError error");
            strlcpy(whyCopy, why, whySize);
            whyCopy[0]=toupper(why[0]);
            fprintf(output_ptr, "%s", colorStr);
            vfprintf(output_ptr, whyCopy, arg);
            fprintf(output_ptr, "%s", COLOR_RESET);
            tryFree(whyCopy);
        }
    }
    pthread_mutex_unlock(mutex_stdout);
}

void fatalError_abort(const char *caller, char *why, ...) {
    va_list arg;
    va_start(arg, why);
    vprintError(caller, HRED, why, arg);
    va_end(arg);
    abort();
}

void fatalError_exit(const char *caller, char *why, ...) {
    va_list arg;
    va_start(arg, why);
    vprintError(caller, HRED, why, arg);
    va_end(arg);
    exit(EXIT_FAILURE);
}

int msleep(unsigned long msec) {
    struct timespec ts;
    int res;

    ts.tv_sec = msec / 1000;
    ts.tv_nsec = (msec % 1000) * 1000000;

    do {
        res = nanosleep(&ts, &ts);
    } while (res && errno == EINTR);

    return res;
}

memBlock *initBlock(){
    memBlock *mem = NULL;
    mallocMacro(mem, sizeof(memBlock), "initBlock error");
    mem->memory=NULL;
    mem->size=0;
    return mem;
}

void freeBlock(memBlock *mem) {
    free(mem->memory);
    free(mem);
}

// curl memory callback
size_t curlMemCb (void *contents, size_t size, size_t quantity, void *obj) {
    size_t realSize = size * quantity;

    memBlock *mem=(memBlock *) obj;
    char *tempPtr=realloc(mem->memory, mem->size+realSize+1);
    if (tempPtr==NULL) {
        fatalError_abort("curlMemCb error: realloc error!\nError: %s\n", strerror(errno));
    } 
    mem->memory=tempPtr;
    memcpy(&(mem->memory[mem->size]), contents, realSize);
    mem->size+=realSize;
    mem->memory[mem->size]='\0';

    return realSize;
}

// perform GET request with curl
CURLcode getRequest(struct progConfig *conf, const char *url, void *data, size_t (*func)(void *contents, size_t size, size_t quantity, void *obj)) {
    CURL *curl=curl_easy_init();
    char errbuf[CURL_ERROR_SIZE];
    curl_easy_setopt(curl, CURLOPT_SHARE, conf->cURLshare);
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, data);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, func);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1);
    curl_easy_setopt(curl, CURLOPT_DNS_CACHE_TIMEOUT, 300);
    curl_easy_setopt(curl, CURLOPT_MAXAGE_CONN, 30);
    //curl_easy_setopt(curl, CURLOPT_USERAGENT, "libcurl-agent/1.0");
    curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errbuf);

    if (curl != NULL) {
        CURLcode ret;
        for (int i=1; i <= 10; i++) { // basically, DOS protection
            errbuf[0]='\0';
            ret=curl_easy_perform(curl);
            if (ret==CURLE_OK){
                curl_easy_cleanup(curl);
                return ret;
            } else if (ret==CURLE_COULDNT_RESOLVE_HOST) {
            }
            printError("getRequest warning", true, HYEL, "URL: \"%s\";\nReturned \"%s\" - sleeping for %dms\n", url, curl_easy_strerror(ret), i*2000);
            msleep(i*2000);
        }
        printError("getRequest warning", true, HYEL, "URL: \"%s\";\nReturned %d:\n\"%s\"\n", url, ret, errbuf);
        curl_easy_cleanup(curl);
        return ret;
    } else {
        curl_easy_cleanup(curl);
        printError("getRequest warning", true, HYEL, "could not init curl, pointer was NULL!\n");
        return -1;
    }
}

size_t dlFileCb(void *ptr, size_t size, size_t nmemb, void *stream) {
    return fwrite(ptr, size, nmemb, (FILE *)stream);
}

// use curl to download file
CURLcode dlFile(progConfig *conf, const char *url, const char *fileName) {
    FILE *file=fopen(fileName, "w");
    if (file==NULL) {
        printError("dlFile warning", true, HYEL, "could not open file: \"%s\";\nError: %s;\n", fileName, strerror(errno));
        return -1;
    }
    CURLcode ret=getRequest(conf, url, file, dlFileCb);
    fclose(file);
    return ret;
}

// check if string matches regex
char **matchReg(const char *str, const char *regStr, int maxMatches) {
    regex_t regComp;
    regmatch_t *matchArr=NULL;
    mallocMacro(matchArr, sizeof(regmatch_t)*maxMatches, "mathReg error");
    if (regcomp(&regComp, regStr, REG_EXTENDED)) {
        fatalError_abort("matchReg error", "could not compile reg expression: \"%s\";\nError: %s;\n", regStr, strerror(errno));
    }
    char **srcCopy=NULL;
    if (regexec(&regComp, str, maxMatches, matchArr, 0)==0) {
        int iter=0;
        for (int i=1; i<=maxMatches; i++ ) {
            if (matchArr[i-1].rm_so == -1)
                break;  // No more groups

            if (srcCopy==NULL) {
                srcCopy=NULL;
                mallocMacro(srcCopy, sizeof(char *)*(i+1), "matchReg error");
                srcCopy[0]=NULL;
            } else {
                char **tmp=srcCopy;
                srcCopy=realloc(tmp, sizeof(char *)*(i+1));
                if (srcCopy==NULL) {
                    fatalError_abort("matchReg error", "could not realloc srcCopy!\nError: %s;\n", strerror(errno));
                }
            }
            srcCopy[i]=NULL;
            int subStrLen=(matchArr[i-1].rm_eo-matchArr[i-1].rm_so)+1;
            if (i==2) {
                char *temp=NULL;
                mallocMacro(temp, subStrLen, "matchReg error");
                strlcpy(temp, str+matchArr[i-1].rm_so, subStrLen);
                char *justFilename = basename(temp);
                subStrLen=strlen(justFilename)+1;
                srcCopy[i]=NULL;
                mallocMacro(srcCopy[i], subStrLen, "matchReg error");
                strlcpy(srcCopy[i], justFilename, subStrLen);
                free(temp);
            } else {
                srcCopy[i]=NULL;
                mallocMacro(srcCopy[i], subStrLen, "matchReg error");
                strlcpy(srcCopy[i], str+matchArr[i-1].rm_so, subStrLen);
            }
            iter=i;
        }
        size_t iterSize=intSize(iter)+1;
        char *tempBuff=NULL;
        mallocMacro(tempBuff, iterSize, "matchReg error");
        tempBuff[0]='\0';
        snprintf(tempBuff, iterSize, "%d", iter);
        srcCopy[0]=NULL;
        mallocMacro(srcCopy[0], iterSize, "matchReg error");
        strlcpy(srcCopy[0], tempBuff, iterSize);
        tryFree(tempBuff);
    }
    regfree(&regComp);
    tryFree(matchArr);
    return srcCopy;
}

// int to string 
void intToStr(char *buff, int num) {
    if (buff==NULL) {
        fatalError_abort("intToStr error", "failed because '*buff' is NULL\n");
    }
    size_t size=intSize(num);
    char *tempBuff=NULL;
    mallocMacro(tempBuff, size+1, "intToStr error");

    tempBuff[0]='\0';
    snprintf(tempBuff, size+1, "%d", num);
    strlcpy(buff, tempBuff, size+1);
    tryFree(tempBuff);
}

// read integer from string
int parseStrToInt(const char *str) {
    if (str==NULL){
        fatalError_abort("parseStrToInt error", "could not convert to int, string is NULL!\n");
    }
    errno=0;
    char *end = NULL;
    long value = strtol(str, &end, 10);
    if (end == str || '\0' != *end || 0 != errno) {
        fatalError_abort("parseStrToInt error", "error converting \"%s\" to int!\nError: %s;\n", str, strerror(errno));
        return -1;
    } else {
        return value;
    }
}

// read double from string
double parseStrToDouble(const char *str) {
    if (str==NULL){
        fatalError_abort("parseStrToDouble error", "could not convert to double, string is NULL!\n");
    }
    errno=0;
    char *end = NULL;
    double value = strtod(str, &end);
    if (end == str || '\0' != *end || 0 != errno) {
        fatalError_abort("parseStrToDouble error", "error converting \"%s\" to double!\nError: %s;\n", str, strerror(errno));
        return -1;
    } else {
        return value;
    }
}

// free string array
void freeStrArr(char **str) {
    if (str!=NULL){
        if (str[0]!=NULL){
            int val = parseStrToInt(str[0]);
            for (int i=0; i<=val; i++){
                free(str[i]);
            }
        }
        free(str);
    }
}

// remove extension from string
char *removeExtension(const char *str) {
    size_t size=strlen(str);
    char *newStr=NULL;
    mallocMacro(newStr, size+1, "removeExtension error");
    strlcpy(newStr, str, size+1);
    char *end = newStr + size;

    while (end > newStr && *end != '.' && *end != '\\' && *end != '/') {
        --end;
    }
    if ((end > newStr && *end == '.') && (*(end - 1) != '\\' && *(end - 1) != '/')) {
        *end = '\0';
    }
    return newStr;
}

// return extension from string
char *getExtension(const char *str) {
    char *dotPos=strrchr(str, '.');
    char *newStr=NULL;
    if (dotPos!=NULL) {
        size_t size=strlen(dotPos);
        mallocMacro(newStr, size+1, "getExtension error");
        newStr[0]='\0';
        if ((*(dotPos + size - 1) != '\\' && *(dotPos + size - 1) != '/')) {
            strlcpy(newStr, dotPos+1, size);
        }
    } else {
        printError("getExtension error", true, HRED, "could not get extension of \"%s\"\n", str);
    }
    return newStr;
}

// replace all occurences of oldStr with newStr in str
char *replaceAll(char *str, const char *oldStr, const char *newStr) {
    if (str==NULL || oldStr==NULL) {
        return NULL;
    }
    size_t oldStrSize=strlen(oldStr);
    if (oldStrSize==0) {
        return NULL;
    }
    if (newStr==NULL) {
        newStr = "";
    }
    size_t newStrSize=strlen(newStr);
    char *temp1=str, *temp2=NULL;
    int counter=0;
    for (; (temp2=strstr(temp1, oldStr)); ++counter) {
        temp1=temp2+oldStrSize;
    }
    size_t newSize=strlen(str)+(newStrSize-oldStrSize)*counter+1;
    char *resStr = temp2 = malloc(newSize);
    if (resStr==NULL || temp2==NULL) {
        fatalError_abort("replaceAll error", "could not malloc;\nError: %s;\n", strerror(errno));
    }

    while (counter--) {
        temp1=strstr(str, oldStr);
        int diffSize=temp1-str;
        strlcpy(temp2, str, diffSize+1);
        temp2+=diffSize;
        strlcpy(temp2, newStr, newStrSize+1);
        temp2+=newStrSize;
        str+=diffSize+oldStrSize; 
    }
    strlcpy(temp2, str, newSize);
    return resStr;
}

void tryFree(void *ptr){
    if (ptr!=NULL){
        free(ptr);
        ptr=NULL;
    }
}

// use mmap to read file to memory
char *fileToMem(const char *filePath) {
    int file = open(filePath, O_RDONLY);

    if (file < 0 ) {
        fatalError_abort("fileToMem error", "could not open: \"%s\";\nError: %s;\n", filePath, strerror(errno));
    }

    struct stat fileProp;
    if (fstat(file, &fileProp)==-1) {
        fatalError_abort("fileToMem error", "could not get filesize of: \"%s\";\nError: %s;\n", filePath, strerror(errno));
    }

    size_t fileSize=fileProp.st_size;
    char *fileMap=NULL;
    if (fileSize>0) {
        fileMap=mmap(NULL, fileSize, PROT_READ, MAP_PRIVATE, file, 0);
        if (fileMap==MAP_FAILED) {
            fatalError_abort("fileToMem error", "could not use 'mmap' on file: \"%s\";\nError: %s;\n", filePath, strerror(errno));
        }
    }
    return fileMap;
}

int freeFileMem(const char *filePath, char *fileStr) {
    struct stat fileProp;
    if (stat(filePath, &fileProp)==-1) {
        fatalError_abort("freeFileMem error", "could not get filesize of: \"%s\";\nError: %s;\n", filePath, strerror(errno));
    }

    size_t fileSize=fileProp.st_size;
    int ret = munmap(fileStr, fileSize);
    if ( ret == -1){
        fatalError_abort("freeFileMem error", "could not use 'munmap' on file: \"%s\";\nError: %s;\n", filePath, strerror(errno));
    }
    return ret;
}

// get number of digits in number
size_t intSize(size_t num) { // literally fastest way to get digits, see stackoverflow
    if (num >= 100000000000000) return 15;
    if (num >= 10000000000000)  return 14;
    if (num >= 1000000000000)   return 13;
    if (num >= 100000000000)    return 12;
    if (num >= 10000000000)     return 11;
    if (num >= 1000000000)      return 10;
    if (num >= 100000000)       return 9;
    if (num >= 10000000)        return 8;
    if (num >= 1000000)         return 7;
    if (num >= 100000)          return 6;
    if (num >= 10000)           return 5;
    if (num >= 1000)            return 4;
    if (num >= 100)             return 3;
    if (num >= 10)              return 2;
    return 1;
}

// calculate relative path from fromPath to toPath
char *getRelativePath(const char *fromPath, const char *toPath) {
    char *relPath=malloc(FILENAME_MAX+1);
    size_t fromPathSize=strlen(fromPath);

    if (fromPath[fromPathSize-1]!='/') {
        char *fromPathCopy=NULL;
        mallocMacro(fromPathCopy, fromPathSize, "getRelativePath error");
        snprintf(fromPathCopy, fromPathSize, "%s", fromPath);
        cwk_path_get_relative(dirname(fromPathCopy), toPath, relPath, FILENAME_MAX);
        tryFree(fromPathCopy);
    } else {
        cwk_path_get_relative(fromPath, toPath, relPath, FILENAME_MAX);
    }

    return relPath;
}

// check if folder exists, otherwise basically "mkdir -p" - REQUIRES forward slash on folder paths  
int checkFolder(const char *filePath, bool runMKDIR) {
    size_t pathSize=strlen(filePath)+1;
    char *path=NULL;
    mallocMacro(path, pathSize, "checkFolder error");
    strlcpy(path, filePath, pathSize);
    if (path[strlen(path)-1]!='/') { // if given filePath is not of an actual directory
        path=dirname(path);
    }
    struct stat statBuff;
    int ret=stat(path, &statBuff);
    if (ret==-1) { 
        if (errno==ENOENT && runMKDIR==true) {
            if (mkdir(path, 0755)!=0 && errno!=EEXIST) {
                if (errno==ENOENT) {
                    path=dirname(path);
                    char *tempStr=NULL;
                    mallocMacro(tempStr, strlen(path)+2, "checkFolder error");
                    snprintf(tempStr, strlen(path)+2, "%s/", path);
                    if (checkFolder(tempStr, true)==0 && checkFolder(filePath, true)==0) {
                    } else {
                        tryFree(tempStr);
                        fatalError_abort("checkFolder error", "could not create folder \"%s\";\nError: %s;\n", path, strerror(errno));
                    }
                    tryFree(tempStr);
                } else {
                    fatalError_abort("checkFolder error", "could not create folder \"%s\";\nError: %s;\n", path, strerror(errno));
                }
            }
        } else if (errno!=EEXIST && errno!=EISDIR) {
            tryFree(path);
            return -1;
        }
    }
    tryFree(path);
    return 0;
}

// generate image with text label 
char *genImage(char *cmd, char *filePath, char *imgLabel) {
    int ret=checkFolder(filePath, true);
    char *retImg=NULL;
    if (ret==0) {
        char *fileName=randStr(16);
        size_t cmdSize=strlen(filePath)+strlen(fileName)+6; // 6=length of ".png" + padding
        char *fullFilePath=NULL;
        mallocMacro(fullFilePath, cmdSize, "genImage error");
        snprintf(fullFilePath, cmdSize, "%s%s.png", filePath, fileName);

        cmdSize+=strlen(cmd)+strlen(imgLabel)+4;
        char *cmdStr=NULL;
        mallocMacro(cmdStr, cmdSize, "genImage error");
        snprintf(cmdStr, cmdSize, "%s\"%s\" %s", cmd, imgLabel, fullFilePath);
        
        resetSTDColors();
        FILE *imgMag=popen(cmdStr, "r");
        if (imgMag!=NULL) {
            retImg=fullFilePath;
        } else {
            printError("genImage warning", true, HYEL, "could not run \"%s\";\nError: %s;\n", cmdStr, strerror(errno));
            tryFree(fullFilePath);
        }

        tryFree(fileName);
        if (pclose(imgMag)!=0) {
            printError("", false, HRED, "%s\n", strerror(errno));
            printError("genImage warning", true,  HYEL, "error while running \"%s\";\nError: %s;\n", cmdStr, strerror(errno));
        }
        tryFree(cmdStr);
    }
    return retImg;
}

void curlLock(CURL *handle, curl_lock_data data, curl_lock_access laccess, void *userptr) {
    while (pthread_mutex_trylock((pthread_mutex_t *) userptr)!=0) {
        msleep(100);
    }
}
 
void curlUnlock(CURL *handle, curl_lock_data data, void *userptr) {
    pthread_mutex_unlock((pthread_mutex_t *) userptr);
}

// returns string of random characters
char *randStr(size_t size) {
    const char dict[62]="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";

    char *str=NULL;
    mallocMacro(str, size+1, "randStr error");
    for (size_t i=0; i<size; i++) {
        str[i]=dict[rand()%62];
    }
    str[size]='\0';
    return str;
}

// finds subtitles for fileStr in filePath 
char *getSubs(progConfig *conf, char *fileStr, char *filePath) {
    char *epSubs=NULL;
    char *fileName=removeExtension(fileStr);
    fileList *subs=find(conf, filePath, (char *[]){fileName, NULL}, FI_MODE, false);

    for (fileList *tempSub=subs; tempSub!=NULL; tempSub=tempSub->next) {
        if (tempSub->dataSize>2) {
            tryFree(fileName);
            fileName=removeExtension(tempSub->data[2]);
            char *tempExtension=getExtension(tempSub->data[2]);
            if (tempExtension==NULL) {
                continue;
            }

            if (strcmp(tempExtension, subExt1)==0) { // if file is srt
                char *folderPath=tempSub->data[1];


                size_t tempStrSize=strlen(folderPath)+strlen(fileName)+strlen(subExt2)+strlen(".")+2;
                char *tempStr=NULL;
                mallocMacro(tempStr, tempStrSize, "getSubs error");
                tempStr[0]='\0';

                //check if folder has /
                if (folderPath[strlen(folderPath)-1]=='/') {
                    snprintf(tempStr, tempStrSize, "%s%s.%s", folderPath, fileName, subExt2);
                } else {
                    snprintf(tempStr, tempStrSize, "%s/%s.%s", folderPath, fileName, subExt2);
                }

                if (access(tempStr, F_OK)!=0) { // file doesn't already exist, so go ahead and convert srt to vtt
                    char *ffmpegCmd="ffmpeg -loglevel fatal -y -nostdin -i";
                    size_t cmdStrSize=strlen(ffmpegCmd)+strlen(" \"\" \"\"")+strlen(tempSub->data[0])+tempStrSize+1;
                    char *cmdStr=NULL;
                    mallocMacro(cmdStr, cmdStrSize, "getSubs error");
                    cmdStr[0]='\0';
                    snprintf(cmdStr, cmdStrSize, "%s \"%s\" \"%s\"", ffmpegCmd, tempSub->data[0], tempStr);

                    resetSTDColors();
                    FILE *cmdRet=popen(cmdStr, "r");
                    if (cmdRet==NULL) {
                        printError("getSubs warning", true, HYEL, "running \"%s %s %s%s.%s\" failed;\nErrno is %d \"%s\"", ffmpegCmd, tempSub->data[0], folderPath, fileName, subExt2, errno, strerror(errno));
                    } else { // Conversion worked!
                        char *subLangFound=strrchr(fileName, '_');
                        char *subLang=NULL;
                        if (subLangFound==NULL){
                            subLang=NULL;
                            mallocMacro(subLang, 4, "getSubs error");
                            strlcpy(subLang, "_en", 4);
                        } else {
                            subLang=subLangFound;
                        }

                        size_t sizeOfJSONStr=strlen("{\"subFile\":\"\",\"lang\":\"\"}")+strlen(subLang)+tempStrSize+2;
                        char *tempJSONStr=NULL;
                        mallocMacro(tempJSONStr, sizeOfJSONStr, "getSubs error");
                        tempJSONStr[0]='\0';

                        if (epSubs!=NULL) {
                            snprintf(tempJSONStr, sizeOfJSONStr, ",{\"subFile\":\"%s\",\"lang\":\"%s\"}", tempStr, subLang+1);
                            size_t newSize=strlen(epSubs)+sizeOfJSONStr+1;
                            epSubs=realloc(epSubs, newSize);
                            if (epSubs==NULL)
                                fatalError_abort("getSubs error: could not realloc;\nError: %s;\n", strerror(errno));

                            strlcat(epSubs, tempJSONStr, newSize);
                        } else {
                            snprintf(tempJSONStr, sizeOfJSONStr, "{\"subFile\":\"%s\",\"lang\":\"%s\"}", tempStr, subLang+1);
                            epSubs=NULL;
                            mallocMacro(epSubs, sizeOfJSONStr, "getSubs error");
                            strlcpy(epSubs, tempJSONStr, sizeOfJSONStr);
                        }
                        if (subLangFound==NULL) {
                            tryFree(subLang);
                        }
                        tryFree(tempJSONStr);
                    }
                    if (pclose(cmdRet)!=0) {
                        printError("getSubs error", false, HRED, "error occured whilst running pclose on \"%s\";\n", cmdStr);
                    }
                    tryFree(cmdStr);
                }
                tryFree(tempStr);
            } else if (strcmp(tempExtension, subExt2)==0) { // if file is vtt & not already in DB
                char *subLangFound=strrchr(fileName, '_');
                char *subLang=NULL;
                if (subLangFound==NULL){
                    subLang=NULL;
                    mallocMacro(subLang, 4, "getSubs error");
                    strlcpy(subLang, "_en", 4);
                } else {
                    subLang=subLangFound;
                }

                size_t sizeOfJSONStr=strlen("{\"subFile\":\"\",\"lang\":\"\"}")+strlen(subLang)+strlen(tempSub->data[0])+2;
                char *tempJSONStr=NULL;
                mallocMacro(tempJSONStr, sizeOfJSONStr, "getSubs error");
                tempJSONStr[0]='\0';

                if (epSubs!=NULL) {
                    snprintf(tempJSONStr, sizeOfJSONStr, ",{\"subFile\":\"%s\",\"lang\":\"%s\"}", tempSub->data[0], subLang+1);
                    size_t newSize=strlen(epSubs)+sizeOfJSONStr+1;
                    epSubs=realloc(epSubs, newSize);
                    if (epSubs==NULL)
                        fatalError_abort("getSubs error: could not realloc;\nError: %s;\n", strerror(errno));
                    
                    strlcat(epSubs, tempJSONStr, newSize);
                } else {
                    snprintf(tempJSONStr, sizeOfJSONStr, "{\"subFile\":\"%s\",\"lang\":\"%s\"}", tempSub->data[0], subLang+1);
                    epSubs=NULL;
                    mallocMacro(epSubs, sizeOfJSONStr, "getSubs error");
                    strlcpy(epSubs, tempJSONStr, sizeOfJSONStr);
                }
                if (subLangFound==NULL) {
                    tryFree(subLang);
                }
                tryFree(tempJSONStr);
            }
            tryFree(tempExtension);
        }
    }
    freeList(subs);
    tryFree(fileName);
    return epSubs;
}

int getTmdbID(char *URLStr, progConfig *conf) {
    int tmdbID=0;
    
    memBlock *mem=initBlock();
    getRequest(conf, URLStr, mem, curlMemCb);

    cJSON *json_root=cJSON_Parse(mem->memory);
    if (json_root!=NULL) {
        cJSON *json_totRes=cJSON_DetachItemFromObject(json_root, "total_results");
        if (json_totRes!=NULL) {
            if (cJSON_GetNumberValue(json_totRes)>0) {
                cJSON *json_item=cJSON_DetachItemFromObject(json_root, "results");
                cJSON *json_obj=cJSON_DetachItemFromArray(json_item, 0);
                cJSON *json=cJSON_DetachItemFromObject(json_obj, "id");
                tmdbID=cJSON_GetNumberValue(json);
                cJSON_Delete(json);
                cJSON_Delete(json_obj);
                cJSON_Delete(json_item);
            }
        } else {
            printError("getTmdbID warning", true, HYEL, "request error, URL: '%s';\njson_totRes==NULL - json_root was:\n", URLStr);
            char *tempStr=cJSON_Print(json_root);
            printError("", true, COLOR_RESET, tempStr);
            printError("", true, HYEL, "\nEND;\n");
            tryFree(tempStr);
        }
        cJSON_Delete(json_totRes);
    } else {
        printError("getTmdbID warning", true, HYEL, "request error, URL: '%s';\njson_root==NULL\n", URLStr);
    }
    tryFree(URLStr);
    cJSON_Delete(json_root);
    freeBlock(mem);

    return tmdbID;
}

char *getPoster(const char *posterURL, progConfig *conf, int prefImgWidth, double prefImgRatio, char *prefImgLang) {
    char *imgURL=NULL;
    memBlock *mem=initBlock();
    getRequest(conf, posterURL, mem, curlMemCb);
    
    cJSON *json_root=cJSON_Parse(mem->memory);
    if (json_root!=NULL) {
        cJSON *json_posters=cJSON_DetachItemFromObject(json_root, "posters");
        if (json_posters!=NULL){
            for (int j=0; j<cJSON_GetArraySize(json_posters) && imgURL==NULL; j++) { // loop through all & check width & check ratio
                cJSON *item=cJSON_DetachItemFromArray(json_posters, j);
                if (item!=NULL) {
                    cJSON *width=cJSON_DetachItemFromObject(item, "width");
                    cJSON *ratio=cJSON_DetachItemFromObject(item, "aspect_ratio");
                    cJSON *lang=cJSON_DetachItemFromObject(item, "iso_639_1");
                    
                    if ((width!=NULL && cJSON_GetNumberValue(width)==prefImgWidth) || (ratio!=NULL && cJSON_GetNumberValue(ratio)==prefImgRatio)) {
                        if ((prefImgLang!=NULL && lang!=NULL && cJSON_GetStringValue(lang)!=NULL && strcmp(cJSON_GetStringValue(lang), prefImgLang)==0) || prefImgLang==NULL) {
                            cJSON *imgStr=cJSON_DetachItemFromObject(item, "file_path");
                            if (imgStr!=NULL && cJSON_GetStringValue(imgStr)!=NULL) {
                                size_t urlLen=strlen(tmdbImg)+strlen(cJSON_GetStringValue(imgStr))+1;
                                imgURL=NULL;
                                mallocMacro(imgURL, urlLen, "getPoster error");
                                imgURL[0]='\0';
                                snprintf(imgURL, urlLen, "%s%s", tmdbImg, cJSON_GetStringValue(imgStr));
                            }
                            cJSON_Delete(imgStr);
                        }
                    }
                    cJSON_Delete(width);
                    cJSON_Delete(ratio);
                    cJSON_Delete(lang);
                } else {
                    printError("getPoster warning", true, HYEL, "request error, URL: '%s';\nitem==NULL - json_posters was:\n", posterURL);
                    char *tempStr=cJSON_Print(json_posters);
                    printError("", true, COLOR_RESET, tempStr);
                    printError("", true, HYEL, "\nEND;\n");
                    tryFree(tempStr);
                }
                cJSON_Delete(item);
            }
            if (imgURL==NULL) {
                printError("getPoster warning", true, HYEL, "request error, URL: '%s';\nCould not find a poster - json_posters was:\n", posterURL);
                char *tempStr=cJSON_Print(json_posters);
                printError("", true, COLOR_RESET, tempStr);
                printError("", true, HYEL, "\nEND;\n");
                tryFree(tempStr);
            } 
        } else {
            printError("getPoster warning", true, HYEL, "request error, URL: '%s';\njson_posters==NULL - json_root was:\n", posterURL);
            char *tempStr=cJSON_Print(json_root);
            printError("", true, COLOR_RESET, tempStr);
            printError("", true, HYEL, "\nEND;\n");
            tryFree(tempStr);
        }
        cJSON_Delete(json_posters);
    } else {
        printError("getPoster warning", true, HYEL, "request error, URL: '%s';\njson_root==NULL\n", posterURL);
    }
    cJSON_Delete(json_root);
    freeBlock(mem);
    return imgURL;
}

char *compressImg(const char *convCmd, char *fileToConvert, bool overwrite) {   
    char *outputFile=NULL;
    if (overwrite) {
        outputFile=fileToConvert;
    } else {
        char *fileExt=getExtension(fileToConvert);
        char *filePath=dirname(fileToConvert);
        size_t fileNameSize=strlen(filePath)+1+16+strlen(fileExt)+1;
        mallocMacro(outputFile, fileNameSize, "compressImg error");
        char *randomString=randStr(16);
        snprintf(outputFile, fileNameSize, "%s/%s.%s", filePath, randomString, fileExt);
        tryFree(randomString);
        tryFree(fileExt);
    }
    size_t cmdStrSize=strlen(convCmd)+strlen(fileToConvert)+strlen(outputFile)+4; // NULL+spaces
    char *cmdStr=NULL;
    mallocMacro(cmdStr, cmdStrSize, "compressImg error");
    snprintf(cmdStr, cmdStrSize, "%s %s %s", convCmd, fileToConvert, outputFile);
    
    resetSTDColors();
    FILE *cmdRet=popen(cmdStr, "r");
    if (cmdRet==NULL) {
        printError("compressImg warning", true, HYEL, "running \"%s %s %s\" failed;\nErrno is %d \"%s\"", convCmd, fileToConvert, outputFile, errno, strerror(errno));
        if (outputFile!=fileToConvert) {
            tryFree(outputFile);
        }
        outputFile=NULL;
    } else { // Conversion worked!
    }
    if (pclose(cmdRet)!=0) {
        printError("compressImg error", false, HRED, "error occured whilst running pclose on \"%s\";\n", cmdStr);
    }
    tryFree(cmdStr);
    return outputFile;
}

// helper funtion for printBitFlags
void printBits(unsigned bits) {
    if (bits>1) {
        printBits(bits/2);
    }
    printf("%u", bits%2);
}

void printBitFlags(unsigned bits) {
    printf("bitflag is: \t0b");
    printBits(bits);
    printf(";\n");
}

int fixMode(progConfig *conf, progFlags flags, const char *toFix, const char *id, const char *poster, const char *newName, bool refreshMode) {
    cJSON *DBptr=NULL;
    const char *dbStr=NULL, *dbPath=NULL;
    if (flags & MOVIES_MODE) {
        DBptr=conf->JSON_moDB;
        dbStr="Movie";
        dbPath=conf->dbNameMovie;
    } else if (flags & SHOWS_MODE) {
        DBptr=conf->JSON_tvDB;
        dbStr="Show";
        dbPath=conf->dbNameTV;
    }
    if (DBptr!=NULL) {
        cJSON *element=NULL;
        cJSON_ArrayForEach(element, DBptr) { // let's check every element of database
            cJSON *this_name=cJSON_GetObjectItem(element, dbStr);
            if (strcmp(cJSON_GetStringValue(this_name), toFix)==0) { // it's a match!
                cJSON *this_id=cJSON_GetObjectItem(element, "ID");
                cJSON *this_poster=cJSON_GetObjectItem(element, "Poster");
                
                if (id!=NULL) {
                    printInfo("fixMode info", false, "setting id for '%s' from '%s' to '%s';\n", toFix, cJSON_GetStringValue(this_id), id);
                    cJSON_SetValuestring(this_id, id);
                } else {
                    id=cJSON_GetStringValue(this_id);
                }
                
                if (newName!=NULL) {
                    printInfo("fixMode info", false, "setting name of '%s' to '%s';\n", toFix, newName);
                    cJSON_SetValuestring(this_name, newName);
                }
                
                if (poster!=NULL) {
                    printInfo("fixMode info", false, "changing poster of '%s' from '%s' to '%s';\n", toFix, cJSON_GetStringValue(this_poster), poster);
                    cJSON_SetValuestring(this_poster, poster);
                } else {
                    if (refreshMode && (flags & FIX_POSTER_MODE)) {
                        const char *IDptr=cJSON_GetStringValue(this_id);
                        if (id!=NULL) {
                            IDptr=id;
                        }
                        int tempID=parseStrToInt(IDptr);
                        if (tempID>0) {
                            if ((flags & MOVIES_MODE) && conf->getMposter) {
                                printInfo("fixMode info", false, "fetching poster for '%s';\n", toFix);
                                poster=getMoviePoster(conf, tempID);
                            } else if ((flags & SHOWS_MODE) && conf->getTVposter) {
                                printInfo("fixMode info", false, "fetching poster for '%s';\n", toFix);
                                poster=getShowPoster(conf, tempID);
                            }
                            if (poster==NULL) {
                                poster="";
                            }
                            printInfo("fixMode info", false, "changing poster of '%s' from '%s' to '%s';\n", toFix, cJSON_GetStringValue(this_poster), poster);
                            cJSON_SetValuestring(this_poster, poster);
                        }
                    }
                }

                if ((flags & SHOWS_MODE) && (flags & FIX_TITLES_MODE) && refreshMode) {
                    int tempID=parseStrToInt(id);
                    cJSON *this_episodes=cJSON_GetObjectItem(element, "Episodes");
                    cJSON *episode=NULL;
                    printInfo("fixMode info", false, "refreshing episode titles of '%s'...\n", cJSON_GetStringValue(this_name));
                    cJSON_ArrayForEach(episode, this_episodes) {
                        cJSON *this_episodeTitle=cJSON_GetObjectItem(episode, "Title");
                        char *seNum=cJSON_GetStringValue(cJSON_GetObjectItem(episode, "Season"));
                        char *epNum=cJSON_GetStringValue(cJSON_GetObjectItem(episode, "Episode"));
                        char *thisEpisodeName=getEpisodeName(conf, tempID, seNum, epNum, conf->TMDBapi);

                        if (thisEpisodeName!=NULL) {
                            cJSON_SetValuestring(this_episodeTitle, thisEpisodeName);
                            tryFree(thisEpisodeName);
                        }
                    }
                }
                char *json=cJSON_Print(DBptr);
                writeCharToFile(json, dbPath);
                printInfo("fixMode info", false,"fixed '%s' in '%s'.\n", toFix, dbPath);
                tryFree(json);
                return 0;
            }
        }
    } else {
        fatalError_exit("fixMode error", "No database to fix!\n");
    }

    return -1; // didn't find anything to fix
}

int writeCharToFile(const char *str, const char *fileStr) {
    FILE *filePtr = fopen(fileStr, "w");
    if (filePtr != NULL) {
        fputs(str, filePtr);
        fclose(filePtr);
    } else {
        fatalError_abort("writeCharToFile error", "could not open '%s';\nErrno: %s;\n", fileStr, strerror(errno)); 
    }
    return 0;
}

long double getElapsedTime() {
    struct timeval currentTime;
    gettimeofday(&currentTime, NULL);
    return (currentTime.tv_sec - timeProgStart.tv_sec)+((long double)(currentTime.tv_usec - timeProgStart.tv_usec)/1000000);
}

// checks if last character in string is '/' and appends it if needed
char *appendSlash(char *origStr) {
    char *newStr=NULL;
    size_t origLen=strlen(origStr);
    if (origStr[origLen]=='/') {
        newStr=origStr;
    } else {
        origLen+=2;
        mallocMacro(newStr, origLen, "cmyflix error");
        snprintf(newStr, origLen, "%s/", origStr);
    }
    return newStr;
}
