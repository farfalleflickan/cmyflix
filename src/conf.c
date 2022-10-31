#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <bsd/string.h>
#include <pthread.h>
#include "utils.h"
#include "conf.h"

progConfig *getConfig(char *srcPath) {
    progConfig *options=calloc(1, sizeof(struct progConfig));
    if (options==NULL) {
        fatalError_abort("getConfig error", "could not calloc;\nError: %s;\n", strerror(errno));
    }
    // set all pointers to NULL
    initAll(options);
    // init all curl things
    curl_global_init(CURL_GLOBAL_ALL);
    options->cURLshare=curl_share_init();
    mallocMacro(options->cURLconnLock, sizeof(pthread_mutex_t), "getConfig error");
    pthread_mutex_init(options->cURLconnLock, NULL);
    
    curl_share_setopt(options->cURLshare, CURLSHOPT_SHARE, CURL_LOCK_DATA_DNS);
    curl_share_setopt(options->cURLshare, CURLSHOPT_SHARE, CURL_LOCK_DATA_SSL_SESSION);
    curl_share_setopt(options->cURLshare, CURLSHOPT_LOCKFUNC, curlLock);
    curl_share_setopt(options->cURLshare, CURLSHOPT_UNLOCKFUNC, curlUnlock);
    curl_share_setopt(options->cURLshare, CURLSHOPT_USERDATA, options->cURLconnLock);
    
    // map config file
    char *fileMap=fileToMem(srcPath);
    if (fileMap==NULL) {
        fatalError_exit("getConfig error", "configuration file \"%s\" is empty!\n%sUse \"cmyflix --gen-config > %s\" to generate default.\n", srcPath, HBLU, srcPath);
    }
    size_t fileSize=strlen(fileMap)+1;
    char *buff=NULL;
    mallocMacro(buff, fileSize, "getConfig error");
    buff[0]='\0';
    size_t properties=0;
    int *offset=calloc(fileSize, sizeof(int));
    if (offset==NULL) {
        fatalError_abort("getConfig error", "could not calloc;\nError: %s;\n", strerror(errno));
    }
    // loop through config and find all "properties"
    for (size_t i=0, mark=0, size=0, j=0; i<fileSize; i++) {
        if (fileMap[i] == '"') {
            mark++;
        } else if ((fileMap[i] == '=' || fileMap[i]==';') && mark<10) { // we might have found a property!
            offset[properties]=size;
            properties++;
            size=0;
        } else if (fileMap[i] == '#') {
            mark+=10;
        } else if (fileMap[i] == '\n') {
            mark=0;
        } else if (mark<=1) {
            size++;
            buff[j]=fileMap[i];
            j++;
        }
    }
    options->properties=properties;
    
    size_t buffSize=strlen(buff);
    int tvReg=-1;
    // loop through config and get value of each property
    for (size_t i=0, pos=0; pos<buffSize; i+=2) {
        char *confKey=NULL;
        mallocMacro(confKey, offset[i]+1, "getConfig error");
        confKey[0]='\0';
        memcpy(confKey, buff+pos, offset[i]);
        confKey[offset[i]]='\0';
        
        char *confValue=NULL;
        mallocMacro(confValue, offset[i+1]+1, "getConfig error");
        confValue[0]='\0';
        memcpy(confValue, buff+pos+offset[i], offset[i+1]);
        confValue[offset[i+1]]='\0';
        
        if (strcmp(confValue, "true")==0){
            confValue[0]=true;
        } else if (strcmp(confValue, "false")==0){
            confValue[0]=false;
        }

        size_t tempPtrSize=strlen(confValue)+1;
        if (strcmp(confKey, "TVpath")==0) {
            options->TVpath=NULL;
            mallocMacro(options->TVpath, tempPtrSize, "getConfig error");
            strlcpy(options->TVpath, confValue, tempPtrSize);
        } else if (strcmp(confKey, "MoviesPath")==0){
            options->MoviesPath=NULL;
            mallocMacro(options->MoviesPath, tempPtrSize, "getConfig error");
            strlcpy(options->MoviesPath, confValue, tempPtrSize);
        } else if (strcmp(confKey, "dbNameMovie")==0){
            options->dbNameMovie=NULL;
            mallocMacro(options->dbNameMovie, tempPtrSize, "getConfig error");
            strlcpy(options->dbNameMovie, confValue, tempPtrSize);
        } else if (strcmp(confKey, "dbNameTV")==0){
            options->dbNameTV=NULL;
            mallocMacro(options->dbNameTV, tempPtrSize, "getConfig error");
            strlcpy(options->dbNameTV, confValue, tempPtrSize);
        } else if (strcmp(confKey, "regexTVgroups")==0){
            options->regexTVgroups=parseStrToInt(confValue);
        } else if (strcmp(confKey, "regexTVnum")==0){
            options->regexTVnum=parseStrToInt(confValue);
            options->regexTVstr=NULL;
            mallocMacro(options->regexTVstr, (1+options->regexTVnum)*sizeof(char *), "getConfig error");
            tvReg=0;
        } else if (strstr(confKey, "regexTVstr")){
            /* add regex to regex array */
            if (tvReg==-1){
                confCleanup(options);
                fatalError_abort("getConfig error", "regexTVstr declared before regexTVnum in the configuration!\n");
            }

            size_t iterSize=intSize(tvReg+1)+strlen("regexTVstr")+1;
            char *tempBuff=NULL;
            mallocMacro(tempBuff, iterSize, "getConfig error"); 
            tempBuff[0]='\0';
            snprintf(tempBuff, iterSize, "regexTVstr%d", tvReg+1);
            if (strcmp(confKey, tempBuff)==0){
                options->regexTVstr[tvReg]=NULL;
                mallocMacro(options->regexTVstr[tvReg], tempPtrSize, "getConfig error");
                strlcpy(options->regexTVstr[tvReg], confValue, tempPtrSize);
            }
            tryFree(tempBuff);
            tvReg++;
        } else if (strcmp(confKey, "TVextraStr")==0){
            options->TVextraStr=NULL;
            mallocMacro(options->TVextraStr, tempPtrSize, "getConfig error");
            strlcpy(options->TVextraStr, confValue, tempPtrSize);
        } else if (strcmp(confKey, "fileLimit")==0){
            options->fileLimit=parseStrToInt(confValue);
        } else if (strcmp(confKey, "regexM")==0){
            options->regexM=NULL;
            mallocMacro(options->regexM, tempPtrSize, "getConfig error");
            strlcpy(options->regexM, confValue, tempPtrSize);
        } else if (strcmp(confKey, "regexHM")==0){
            options->regexHM=NULL;
            mallocMacro(options->regexHM, tempPtrSize, "getConfig error");
            strlcpy(options->regexHM, confValue, tempPtrSize);
        } else if (strcmp(confKey, "TVhtml")==0){
            options->TVhtml=NULL;
            mallocMacro(options->TVhtml, tempPtrSize, "getConfig error");
            strlcpy(options->TVhtml, confValue, tempPtrSize);
        } else if (strcmp(confKey, "showHTMLFolder")==0){
            options->showHTMLFolder=NULL;
            mallocMacro(options->showHTMLFolder, tempPtrSize, "getConfig error");
            strlcpy(options->showHTMLFolder, confValue, tempPtrSize);
        } else if (strcmp(confKey, "Mhtml")==0){
            options->Mhtml=NULL;
            mallocMacro(options->Mhtml, tempPtrSize, "getConfig error");
            strlcpy(options->Mhtml, confValue, tempPtrSize);
        } else if (strcmp(confKey, "dTVImg")==0){
            options->dTVImg=confValue[0];
        } else if (strcmp(confKey, "dTVFolder")==0){
            options->dTVFolder=NULL;
            mallocMacro(options->dTVFolder, tempPtrSize, "getConfig error");
            strlcpy(options->dTVFolder, confValue, tempPtrSize);
        } else if (strcmp(confKey, "AutogenImgResizeTVCmd")==0){
            options->AutogenImgResizeTVCmd=NULL;
            mallocMacro(options->AutogenImgResizeTVCmd, tempPtrSize, "getConfig error");
            strlcpy(options->AutogenImgResizeTVCmd, confValue, tempPtrSize);
        } else if (strcmp(confKey, "compressImgTVCmd")==0){
            options->compressImgTVCmd=NULL;
            mallocMacro(options->compressImgTVCmd, tempPtrSize, "getConfig error");
            strlcpy(options->compressImgTVCmd, confValue, tempPtrSize);
        } else if (strcmp(confKey, "compressImgTV")==0){
            options->compressImgTV=confValue[0];
        } else if (strcmp(confKey, "prefImgWidthTV")==0){
            options->prefImgWidthTV=parseStrToInt(confValue);
        } else if (strcmp(confKey, "prefImgRatioTV")==0){
            options->prefImgRatioTV=parseStrToDouble(confValue);
        } else if (strcmp(confKey, "prefImgLangTV")==0){
            options->prefImgLangTV=NULL;
            mallocMacro(options->prefImgLangTV, tempPtrSize, "getConfig error");
            strlcpy(options->prefImgLangTV, confValue, tempPtrSize);
        } else if (strcmp(confKey, "dMoImg")==0){
            options->dMoImg=confValue[0];
        } else if (strcmp(confKey, "dMoFolder")==0){
            options->dMoFolder=NULL;
            mallocMacro(options->dMoFolder, tempPtrSize, "getConfig error");
            strlcpy(options->dMoFolder, confValue, tempPtrSize);
        } else if (strcmp(confKey, "AutogenImgResizeMoCmd")==0){
            options->AutogenImgResizeMoCmd=NULL;
            mallocMacro(options->AutogenImgResizeMoCmd, tempPtrSize, "getConfig error");
            strlcpy(options->AutogenImgResizeMoCmd, confValue, tempPtrSize);
        } else if (strcmp(confKey, "compressImgMoCmd")==0){
            options->compressImgMoCmd=NULL;
            mallocMacro(options->compressImgMoCmd, tempPtrSize, "getConfig error");
            strlcpy(options->compressImgMoCmd, confValue, tempPtrSize);
        } else if (strcmp(confKey, "compressImgMo")==0){
            options->compressImgMo=confValue[0];
        } else if (strcmp(confKey, "prefImgWidthM")==0){
            options->prefImgWidthM=parseStrToInt(confValue);
        } else if (strcmp(confKey, "prefImgRatioM")==0){
            options->prefImgRatioM=parseStrToDouble(confValue);
        } else if (strcmp(confKey, "prefImgLangM")==0){
            options->prefImgLangM=NULL;
            mallocMacro(options->prefImgLangM, tempPtrSize, "getConfig error");
            strlcpy(options->prefImgLangM, confValue, tempPtrSize);
        } else if (strcmp(confKey, "getTVposter")==0){
            options->getTVposter=confValue[0];
        } else if (strcmp(confKey, "getEpisodeName")==0){
            options->getEpisodeName=confValue[0];
        } else if (strcmp(confKey, "createTVsubs")==0){
            options->createTVsubs=confValue[0];
        } else if (strcmp(confKey, "getMposter")==0){
            options->getMposter=confValue[0];
        } else if (strcmp(confKey, "createMsubs")==0){
            options->createMsubs=confValue[0];
        } else if (strcmp(confKey, "homeMovies")==0){
            options->homeMovies=confValue[0];
        } else if (strcmp(confKey, "TMDBapi")==0){
            options->TMDBapi=NULL;
            mallocMacro(options->TMDBapi, tempPtrSize, "getConfig error");
            strlcpy(options->TMDBapi, confValue, tempPtrSize);
        } else {
            fatalError_exit("getConfig error", "configuration file \"%s\" contains errors!\nInvalid: \"%s\";\n%sUse \"cmyflix --gen-config > %s\" to generate default.\n", srcPath, confKey, HBLU, srcPath);
        }
        if (i<properties) {
            pos+=offset[i]+offset[i+1];
        } else {
            pos=buffSize;
        }
        tryFree(confValue);
        tryFree(confKey);
    }
    if (buffSize==0) {
        fatalError_exit("getConfig error", "configuration file \"%s\" contains errors!\n%sUse \"cmyflix --gen-config > %s\" to generate default.\n", srcPath, HBLU, srcPath);
    }

    // TMDB api not set/incorrect length value, disable all related options
    if ((options->TMDBapi==NULL || strlen(options->TMDBapi)!=32) && (options->getTVposter || options->getMposter || options->getEpisodeName)) {
       options->getTVposter=false;
       options->getMposter=false;
       options->getEpisodeName=false;
       printError("getConfig warning", false, HYEL, "TMDBapi invalid, disabling all related features...\n");
    }
    
    freeFileMem(srcPath, fileMap);
    tryFree(offset);
    tryFree(buff);

    options->tvDB_exists=false;
    options->moDB_exists=false;

    if( access(options->dbNameTV, W_OK|R_OK) == -1 && errno!=ENOENT ) {
        confCleanup(options);
        fatalError_abort("getConfig error", "missing permissions for \"%s\";\nError: %s;\n", options->dbNameTV, strerror(errno));
    } else {
        // dbNameTV exists, load it to memory and parse the JSON
        if (access(options->dbNameTV, F_OK)==0) {
            options->JSON_tvDB=NULL;
            options->tvDB_str=fileToMem(options->dbNameTV);
            options->JSON_tvDB=cJSON_Parse(options->tvDB_str);
            if (cJSON_GetErrorPtr()==NULL) {
                options->tvDB_exists=true;
            } else {
                printError("getConfig warning", false, HYEL, "error whilst parsing %s, will rebuild DB...\n", options->dbNameTV);
            }
        }
    }

    if( access(options->dbNameMovie, W_OK|R_OK) == -1 && errno!=ENOENT ) {
        confCleanup(options);
        fatalError_abort("getConfig error", "missing permissions for \"%s\";\nError: %s;\n", options->dbNameMovie, strerror(errno));
    } else {
        // dbNameMovie exists, load it to memory and parse the JSON
        if (access(options->dbNameMovie, F_OK)==0) {
            options->JSON_moDB=NULL;
            options->moDB_str=fileToMem(options->dbNameMovie);
            options->JSON_moDB=cJSON_Parse(options->moDB_str);
            if (cJSON_GetErrorPtr()==NULL) {
                options->moDB_exists=true;
            } else {
                printError("getConfig warning", false, HYEL, "error whilst parsing %s, will rebuild DB...\n", options->dbNameMovie);
            }
        }
    }
    // set ulimit
    if (options->fileLimit>0) {
        if (getrlimit(RLIMIT_NOFILE, &(options->defLim))==0) {
            if (options->defLim.rlim_cur<options->fileLimit) {
                options->newLim.rlim_max=options->defLim.rlim_max;
                options->newLim.rlim_cur=options->fileLimit;

                if (setrlimit(RLIMIT_NOFILE, &(options->newLim))==-1){
                    confCleanup(options);
                    fatalError_abort("getConfig error", "could not set new fileLimit, error: %s\n", errno, strerror(errno));
                }
            }
        }
    }
    // prepare PRNG for msleep
    srand(time(NULL));

    return options;
}

// free everything that conf allocates
void confCleanup(progConfig *options) {
    if (options->tvDB_str!=NULL) {
        freeFileMem(options->dbNameTV, options->tvDB_str);
    }
    if (options->moDB_str!=NULL) {
        freeFileMem(options->dbNameMovie, options->moDB_str);
    }
    if (options->JSON_moDB!=NULL) {
        cJSON_Delete(options->JSON_moDB);
    }
    if (options->JSON_tvDB!=NULL) {
        cJSON_Delete(options->JSON_tvDB);
    }
    tryFree(options->TVpath);
    tryFree(options->MoviesPath);
    tryFree(options->dbNameMovie);
    tryFree(options->dbNameTV);
    for (int i=0; i<options->regexTVnum; i++) {
        tryFree(options->regexTVstr[i]);
    }
    tryFree(options->regexTVstr);
    tryFree(options->TVextraStr);
    tryFree(options->regexM);
    tryFree(options->regexHM);
    tryFree(options->TVhtml);
    tryFree(options->showHTMLFolder);
    tryFree(options->Mhtml);
    tryFree(options->dTVFolder);
    tryFree(options->AutogenImgResizeTVCmd);
    tryFree(options->prefImgLangTV);
    tryFree(options->dMoFolder);
    tryFree(options->AutogenImgResizeMoCmd);
    tryFree(options->prefImgLangM);
    tryFree(options->TMDBapi);
    tryFree(options->compressImgMoCmd);
    tryFree(options->compressImgTVCmd);
    
    curl_share_cleanup(options->cURLshare);
    pthread_mutex_destroy(options->cURLconnLock);
    tryFree(options->cURLconnLock);
    curl_global_cleanup();
    tryFree(options);
}

// initialize all ptr of *options to NULL
void initAll(progConfig *options) {
    if (options!=NULL) {
        options->cURLshare=NULL;
        options->cURLconnLock=NULL;
        options->TVpath=NULL;
        options->MoviesPath=NULL;
        options->dbNameMovie=NULL;
        options->dbNameTV=NULL;
        options->regexTVstr=NULL;
        options->TVextraStr=NULL;
        options->regexM=NULL;
        options->regexHM=NULL;
        options->TVhtml=NULL;
        options->showHTMLFolder=NULL;
        options->Mhtml=NULL;
        options->dTVFolder=NULL;
        options->AutogenImgResizeTVCmd=NULL;
        options->dMoFolder=NULL;
        options->AutogenImgResizeMoCmd=NULL;
        options->TMDBapi=NULL;
        options->regexTVnum=-1;
        options->getMposter=NULL;
        options->getTVposter=NULL;
        options->tvDB_str=NULL;
        options->moDB_str=NULL;
        options->prefImgLangM=NULL;
        options->prefImgLangTV=NULL;
        options->JSON_moDB=NULL;
        options->JSON_tvDB=NULL;
    } else {
        fatalError_exit("initAll", "options is NULL.\n");
    }
}
