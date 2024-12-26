#include <stdbool.h>
#include <stdlib.h>
#include <pthread.h>
#include <bsd/string.h> 
#include <cjson/cJSON.h>
#include <unistd.h>
#include <errno.h>
#include "tvshow.h"
#include "fileList.h"
#include "thread.h"
#include "iofiles.h"
#include "utils.h"


// JSON maker
struct fileList *createTVShowDB(progConfig *conf) {
    printInfo("createTVShowDB info", true, "building TV shows' database...\n");
    fileList *showFolders=find(conf, conf->TVpath, "", DI_MODE, false);

    if (showFolders != NULL ) {
        int foldersListSize=showFolders->listSize, i=0;
        pthread_t *threads=NULL;
        mallocMacro(threads, sizeof(pthread_t)*foldersListSize, "createTVShowDB error");
        
        threadStruct *threadObj=NULL;
        mallocMacro(threadObj, sizeof(threadStruct)*foldersListSize, "createTVShowDB error");
        cJSON *tempJSON=NULL;   
        // open JSON database/set up cJSON
        if (conf->tvDB_exists==true) {
            tempJSON=cJSON_Duplicate(conf->JSON_tvDB, true);
        }
        for (fileList *tempList = showFolders; tempList != NULL; tempList = tempList->next, i++) {
            threadObj[i].conf=conf;
            threadObj[i].data=tempList->data;
            threadObj[i].oldJSON=NULL;
            threadObj[i].list=NULL;
            threadObj[i].id=0;

            if (conf->tvDB_exists==true && tempJSON!=NULL) {
                char *tvShowName=replaceAll(tempList->data[1], ".", " ");
                cJSON *item=NULL;

                cJSON_ArrayForEach(item, tempJSON){
                    if (cJSON_GetObjectItem(item, "show")!=NULL && cJSON_GetStringValue(cJSON_GetObjectItem(item, "show"))!=NULL) {
                        char *tempStr=cJSON_GetStringValue(cJSON_GetObjectItem(item, "show"));
                        if (strcmp(tempStr, tvShowName)==0) { // match
                            threadObj[i].id=parseStrToInt(cJSON_GetStringValue(cJSON_GetObjectItem(item, "ID")));
                            threadObj[i].oldJSON=cJSON_DetachItemViaPointer(tempJSON, item);
                            break;
                        }
                    } else {
                        printError("createTVShowDB warning", false, HYEL, "some errors occured while reading \"%s\", parts of the database will be rebuilt from scratch...\n", conf->dbNameTV);
                        threadObj[i].oldJSON=NULL;
                        conf->tvDB_exists=false;
                        cJSON_Delete(tempJSON);
                        tempJSON=NULL;
                        break;
                    }
                }
                tryFree(tvShowName);
            }

            pthread_create(&threads[i], NULL, findSeasons, (void *) &threadObj[i]);
        }

        fileList *showJSON=NULL;
        for (i=0; i<foldersListSize;i++) { // loop through all found tv shows
            pthread_join(threads[i], NULL);

            showJSON=joinLists(showJSON, threadObj[i].list);
            if (threadObj[i].oldJSON!=NULL){
                cJSON_Delete(threadObj[i].oldJSON);
            }
        }
        if (tempJSON!=NULL){
            cJSON_Delete(tempJSON);
        }
        tryFree(threads);
        tryFree(threadObj);
        freeList(showFolders);
        
        // sort shows alphabetically
        msortList(&showJSON, 1, true, STRING);

        for (fileList *temp=showJSON; temp!=NULL; temp=temp->next) { // remove temp->data[1] (which should be the tv show name), meanwhile temp->data[0] is the whole show JSON 
            tryFree(temp->data[temp->dataSize-1]);
            temp->dataSize--;
        }
        char *jsonStr=fileListToJSONStr(showJSON);
        if (jsonStr!=NULL) {
            cJSON_Delete(conf->JSON_tvDB);
            conf->JSON_tvDB=cJSON_Parse(jsonStr);
        }
        tryFree(jsonStr);

        return showJSON;
    } else {
        printError("createTVShowDB warning", false, HYEL, "could not find anything in \"%s\";\n", conf->TVpath);
    }
    return NULL;   
}

void *findSeasons(void *threadArg) { // runs in Show.Name folder and spawns threads in every season folder
    threadStruct *thisThread=(threadStruct *) threadArg;
    progConfig *conf=thisThread->conf;


    size_t lenStr1=strlen(thisThread->data[0]);
    size_t lenStr2=strlen(thisThread->data[1]); // thisThread->data[1] name of tv show top folder
    char *showPath=NULL;
    mallocMacro(showPath, lenStr1+lenStr2+2, "findSeasons error");
    strlcpy(showPath, thisThread->data[0], lenStr1+1);
    if (showPath[lenStr1 - 1] != '/') {
        strlcat(showPath, "/",  lenStr1+lenStr2+2);
    }
    strlcat(showPath, thisThread->data[1], lenStr1+lenStr2+2);
    if (thisThread->data[1][lenStr2-1] != '/') {
        strlcat(showPath, "/", lenStr1+lenStr2+2);
    }
    // Find all the seasons of the current tv show
    printInfo("findSeasons info", true, "looking for seasons' folders in: \"%s\";\n", showPath);
    fileList *seasonsList = find(conf, showPath, "", DI_MODE, false); // get all seasons folders
    
    if (seasonsList!=NULL && seasonsList->data!=NULL) {
        bool showHasExtras=false;
        pthread_t *threads=NULL;
        mallocMacro(threads, sizeof(pthread_t)*seasonsList->listSize, "findSeasons error");
        threadStruct *threadObj=NULL;
        mallocMacro(threadObj, sizeof(threadStruct)*seasonsList->listSize, "findSeasons error");
        // TV show stuff 
        int tmdb_id=thisThread->id;
        char *imgURL=NULL;

        char *tvShowName=replaceAll(thisThread->data[1], ".", " ");
        if (tmdb_id==0) {
            tmdb_id=getShowID(conf, tvShowName); // get the ID for this show
        }

        int i=0;
        for (fileList *tempList=seasonsList; tempList!=NULL; tempList=tempList->next, i++) { // run thread in every season folder
            size_t tempPathSize=strlen(showPath)+strlen(tempList->data[1])+2;
            char *tempPath=NULL; // path to season
            mallocMacro(tempPath, tempPathSize, "findSeasons error");
            strlcpy(tempPath, showPath, tempPathSize);
            strlcat(tempPath, tempList->data[1], tempPathSize);
            if (tempPath[strlen(tempPath)-1]!='/') {
                strlcat(tempPath, "/", tempPathSize);
            }
            if (strstr(tempPath, conf->TVextraStr)) {
                showHasExtras=true;
            }

            // search for episodes & stuff
            threadObj[i].conf=thisThread->conf;
            threadObj[i].oldJSON=thisThread->oldJSON;
            threadObj[i].list=NULL;
            threadObj[i].data=NULL;
            mallocMacro(threadObj[i].data, sizeof(char **), "findSeasons error");
            size_t tempSize=strlen(tempPath)+1;
            threadObj[i].data[0]=NULL;
            mallocMacro(threadObj[i].data[0], tempSize, "findSeasons error");
            strlcpy(threadObj[i].data[0], tempPath, tempSize);
            threadObj[i].id=tmdb_id;

            pthread_create(&threads[i], NULL, findEpisodes, (void *) &threadObj[i]);
            tryFree(tempPath);
        }
        // get TV show poster!
        if (thisThread->oldJSON!=NULL) {
            char *tempStr=cJSON_GetStringValue(cJSON_GetObjectItem(thisThread->oldJSON, "Poster"));
            size_t tempStrLen=strlen(tempStr)+1;
            if (tempStrLen>3) {
                imgURL=NULL;
                mallocMacro(imgURL, tempStrLen, "findSeasons error");
                strlcpy(imgURL, tempStr, tempStrLen);
            }
        }
        if (imgURL==NULL && thisThread->conf->getTVposter==true && tmdb_id>0) { 
            imgURL=getShowPoster(thisThread->conf, tmdb_id);
        } 
        if (imgURL==NULL) { // if still NULL, getShowPoster didn't work... 
            imgURL=genImage(conf->AutogenImgResizeTVCmd, conf->dTVFolder, tvShowName);
            /*
            // useless on genImage? compressed results bigger than original
            if (conf->compressImgTV) {
                imgURL=compressImg(conf->compressImgTVCmd, imgURL, true);
            }
            */
        }

        for (int j=0; j<i; j++) {
            pthread_join(threads[j], NULL);
            thisThread->list=joinLists(thisThread->list, threadObj[j].list);

            tryFree(threadObj[j].data[0]);
            tryFree(threadObj[j].data);
        }

        if (thisThread->list!=NULL) {
            if (thisThread->list->listSize>1) {
                msortList(&thisThread->list, 0, true, INTEGER);
            }

            size_t numOfSeasons=seasonsList->listSize;
            if (showHasExtras) {
                numOfSeasons--;
            }
            if (imgURL==NULL) {
                mallocMacro(imgURL, 1, "findSeasons error");
                imgURL[0]='\0';
            }
            size_t showJSONStrSize=strlen("{\"Show\":\"\",\"ID\":\"\",\"Poster\":\"\",\"Seasons\":\"\",\"Episodes\":[],\"Extras\":[]}")+strlen(tvShowName)+intSize(tmdb_id)+strlen(imgURL)+intSize(numOfSeasons)+1;
            char *showJSONStr=NULL;
            mallocMacro(showJSONStr, showJSONStrSize, "findSeasons error");
            showJSONStr[0]='\0';
            snprintf(showJSONStr, showJSONStrSize, "{\"Show\":\"%s\",\"ID\":\"%d\",\"Poster\":\"%s\",\"Seasons\":\"%zu\",\"Episodes\":[", tvShowName, tmdb_id, imgURL, numOfSeasons);

            i=0;
            size_t extrasJSONStrSize=2, j=0;
            char *extrasJSONStr=NULL;
            mallocMacro(extrasJSONStr, extrasJSONStrSize, "findSeasons error");
            extrasJSONStr[0]='\0';
            for (fileList *tempList=thisThread->list; tempList!=NULL; tempList=tempList->next) {
                if (tempList->dataSize>=2 && tempList->data[1]!=NULL) {
                    if (strstr(tempList->data[1], conf->TVextraStr)!=NULL) { // check if this list comes from a 'extras season'
                        for (size_t k=1; k<tempList->dataSize; k++) { // iterate over all files in the 'extras' folder
                            extrasJSONStrSize+=strlen(tempList->data[k]);
                            extrasJSONStr=realloc(extrasJSONStr, extrasJSONStrSize);
                            if (extrasJSONStr==NULL)
                                fatalError_abort("findSeasons error", "could not realloc;\nError: %s;\n", strerror(errno));

                            if (j>0) {
                                extrasJSONStrSize++;
                                strlcat(extrasJSONStr, ",", extrasJSONStrSize);
                            }
                            strlcat(extrasJSONStr, tempList->data[k], extrasJSONStrSize);
                            j++;
                        }
                    } else { //list from a regular season, not extras
                        for (size_t k=1; k<tempList->dataSize; k++) {
                            showJSONStrSize+=strlen(tempList->data[k]);
                            showJSONStr=realloc(showJSONStr, showJSONStrSize);
                            if (showJSONStr==NULL)
                                fatalError_abort("findSeasons error", "could not realloc;\nError: %s;\n", strerror(errno));

                            if (i>0) {
                                showJSONStrSize++;
                                strlcat(showJSONStr, ",", showJSONStrSize);
                            }
                            strlcat(showJSONStr, tempList->data[k], showJSONStrSize);
                            i++;
                        }
                    }
                }
            }
            char *tempShowJSONStr=NULL;
            mallocMacro(tempShowJSONStr, showJSONStrSize, "findSeasons error");
            strlcpy(tempShowJSONStr, showJSONStr, showJSONStrSize);
            showJSONStr=realloc(showJSONStr, showJSONStrSize+extrasJSONStrSize);
            if (showJSONStr==NULL) {
                fatalError_abort("findSeasons error", "could not realloc;\nError: %s;\n", strerror(errno));
            }
            showJSONStr[0]='\0';
            snprintf(showJSONStr, showJSONStrSize+extrasJSONStrSize, "%s],\"Extras\":[%s]}", tempShowJSONStr, extrasJSONStr);
            freeList(thisThread->list);
            thisThread->list=newList();
            addData(thisThread->list, showJSONStr);
            addData(thisThread->list, tvShowName);

            tryFree(threads);
            tryFree(threadObj);
            freeList(seasonsList);
            tryFree(imgURL);
            tryFree(showJSONStr);
            tryFree(tvShowName);
            tryFree(extrasJSONStr);
            tryFree(tempShowJSONStr);
        } else {
            printError("findSeasons warning", false, HYEL, "please note, could not find any \".%s\" in path \"%s\"\n", videoExt, showPath);
        }
    } else {
        printError("findSeasons warning", false, HYEL, "please note, path \"%s\" is empty!\n", showPath);
    }
    tryFree(showPath);
    return NULL;
}

void *findEpisodes(void *threadArg) { // find files of 'this' season
    threadStruct *thisThread = (threadStruct *) threadArg;
    fileList *foundFiles=NULL;
    ioMode mode=TV_MODE | FI_MODE;
    if (strstr(thisThread->data[0], thisThread->conf->TVextraStr)!=NULL) {
        mode=FI_MODE;
    }

    printInfo("findEpisodes info", true, "looking for episodes in: \"%s\";\n", thisThread->data[0]);
    foundFiles=find(thisThread->conf, thisThread->data[0], videoExt, mode, false); // search for video files in this folder
    if (foundFiles==NULL) {
        thisThread->list=NULL;
    } else {
        size_t thisShowSeason=9999999; // "No season" as start value, because of sorting

        if ((mode & TV_MODE)!=0) { //  if mode has TV_MODE bit set then we are NOT in the folder of the extras
            msortList(&foundFiles, 5, true, INTEGER); // sort list by data[5] ( episode number )
            if (foundFiles->dataSize>4) { // getting season & not in Season.Extras
                thisShowSeason=parseStrToInt(foundFiles->data[4]);
            } else { // could not get current season from filename, trying by getting it from folder name 
                if (foundFiles->dataSize>1) {
                    // this only works if folder is named using the 'Something.1' format
                    char *tempStrPtr=strrchr(foundFiles->data[1], '.');
                    size_t tempStrPtrLen=strlen(tempStrPtr);
                    char *tempStr=NULL;
                    mallocMacro(tempStr, tempStrPtrLen, "findEpisodes error");
                    tempStr[0]='\0';
                    if (tempStrPtr[tempStrPtrLen-1]=='/') {
                        tempStrPtrLen--;
                    }
                    strlcpy(tempStr, tempStrPtr+1, tempStrPtrLen);
                    thisShowSeason=parseStrToInt(tempStr);
                }     
            }
        }
        fileList *jsonObjList=newList();

        unsigned int showId=thisThread->id;
        size_t size=intSize(thisShowSeason)+1;
        char *tempStr=NULL;
        mallocMacro(tempStr, size, "findEpisodes error");
        tempStr[0]='\0';
        intToStr(tempStr, thisShowSeason);
        addData(jsonObjList, tempStr);

        for (fileList *temp=foundFiles; temp!=NULL; temp=temp->next) { //iterate through episodes
            char *epNum=NULL, *seNum=NULL, *epName=NULL, *epSubs=NULL;
            bool seasonExtra=false;

            if (thisThread->conf->tvDB_exists && strstr(thisThread->conf->tvDB_str, temp->data[0])) {
                if (thisThread->oldJSON!=NULL){
                    cJSON *JSONcopy=cJSON_Duplicate(thisThread->oldJSON, true);
                    cJSON *episodesArray=cJSON_GetObjectItem(JSONcopy, "episodes");
                    cJSON *item=NULL;
                    
                    if (episodesArray!=NULL) {
                        cJSON_ArrayForEach(item, episodesArray) {
                            if (item!=NULL) {
                                cJSON *obj=cJSON_GetObjectItem(item, "File");
                                if (obj!=NULL) {
                                    char *fileStr=cJSON_GetStringValue(obj);
                                    if (fileStr!=NULL && strcmp(fileStr, temp->data[0])==0) { //match
                                        item=cJSON_DetachItemViaPointer(episodesArray, item);
                                        char *tempSeNum=cJSON_GetStringValue(cJSON_GetObjectItem(item, "Season"));
                                        char *tempEpNum=cJSON_GetStringValue(cJSON_GetObjectItem(item, "Episode"));
                                        char *tempEpName=cJSON_GetStringValue(cJSON_GetObjectItem(item, "Title"));
                                        size_t tempSize=0;

                                        tempSize=strlen(tempSeNum)+1;
                                        if (tempSize>1) { 
                                            seNum=NULL;
                                            mallocMacro(seNum, tempSize, "findEpisodes error");
                                            strlcpy(seNum, tempSeNum, tempSize);
                                        }

                                        tempSize=strlen(tempEpNum)+1;
                                        if (tempSize>1) { 
                                            epNum=NULL;
                                            mallocMacro(epNum, tempSize, "findEpisodes error");
                                            strlcpy(epNum, tempEpNum, tempSize);
                                        }
                                        tempSize=strlen(tempEpName)+1;
                                        if (tempSize>1) { 
                                            epName=NULL;
                                            mallocMacro(epName, tempSize, "findEpisodes error");
                                            strlcpy(epName, tempEpName, tempSize);
                                        }
                                        cJSON_Delete(item);
                                        break;
                                    }
                                }
                            }
                        }
                        cJSON_Delete(JSONcopy);
                    }
                //} else {
                //    printError("File \"%s\" found in tvDB_str but oldJSON==NULL\n", temp->data[0]);
                }
            }

            if (temp->data!=NULL && strstr(temp->data[0], thisThread->conf->TVextraStr)!=NULL) {
                seasonExtra=true;
                epName=removeExtension(temp->data[2]); // episode name is filename without extension
            } else { // get episodes' names
                /* temp->data 'format'
                 * temp->data[0] full path to file ( ex: ../TV/Show/Season/Show.S01E01.TheOneThatIsAnExample.mp4 )
                 * temp->data[1] path to file's folder ( ex: ../TV/Show/Season/ )
                 * temp->data[2] filename ( ex: Show.S01E01.TheOneThatIsAnExample.mp4 )
                 now filename is split into regex groups... Default would be something like the following
                 * temp->data[3] Show
                 * temp->data[4] 01
                 * temp->data[5] 01
                 * temp->data[6] TheOneThatIsAnExample.mp4
                 */            
                if (temp->dataSize > 5 ) { // use regexTVgroups instead of hard coded number? See temp->data 'format'
                    if (seNum==NULL) {
                        size_t tempStrLen=strlen(temp->data[4])+1;
                        seNum=NULL;
                        mallocMacro(seNum, tempStrLen, "findEpisodes error");
                        strlcpy(seNum, temp->data[4], tempStrLen);
                    }

                    if (epNum==NULL) {
                        size_t tempStrLen=strlen(temp->data[5])+1;
                        epNum=NULL;
                        mallocMacro(epNum, tempStrLen, "findEpisodes error");
                        strlcpy(epNum, temp->data[5], tempStrLen);
                    }

                    if (epNum!=NULL && seNum!=NULL && epName==NULL && thisThread->conf->getEpisodeName==true && showId!=0 ) {
                        epName=getEpisodeName(thisThread->conf, showId, seNum, epNum, thisThread->conf->TMDBapi);  
                    }                
                } else { // dataSize got weird, shouldn't really happen?
                    if (temp->dataSize>0){
                        printError("findEpisodes warning", false, HYEL, "file \"%s\" has weird dataSize: %d;\n", temp->data[0], temp->dataSize);
                    }
                }
            }

            if (thisThread->conf->createTVsubs) { // search for subs, convert srt to vtt - no matter if already in DB or not
                if (temp->dataSize>2) {
                    epSubs=getSubs(thisThread->conf, temp->data[2], thisThread->data[0]);
                }
            }

            if (epName==NULL) {
                if (temp->dataSize>5 && seasonExtra==false) {
                    if (strcmp(temp->data[6], videoExt)==0) { 
                        epName=removeExtension(temp->data[2]);
                    } else {
                        epName=removeExtension(temp->data[6]);
                    }
                } else {
                    epName=removeExtension(temp->data[2]);
                }
            }
            if (epSubs==NULL) {
                epSubs=NULL;
                mallocMacro(epSubs, 1, "findEpisodes error");
                epSubs[0]='\0';
            }
            if (seNum==NULL) {
                seNum=NULL;
                mallocMacro(seNum, 2, "findEpisodes error");
                seNum[0]='0';
                seNum[1]='\0';
            }
            if (epNum==NULL) {
                epNum=NULL;
                mallocMacro(epNum, 2, "findEpisodes error");
                epNum[0]='0';
                epNum[1]='\0';
            }
            
            char *epJSON=NULL;
            if (seasonExtra) {
                size_t epJSONStrSize=strlen("{\"Title\":\"\",\"File\":\"\",\"Subs\":[]}")+strlen(seNum)+strlen(epNum)+strlen(epName)+strlen(temp->data[0])+strlen(epSubs)+1;
                epJSON=NULL;
                mallocMacro(epJSON, epJSONStrSize, "findEpisodes error");
                epJSON[0]='\0';
                snprintf(epJSON, epJSONStrSize, "{\"Title\":\"%s\",\"File\":\"%s\",\"Subs\":[%s]}", epName, temp->data[0], epSubs);
                addData(jsonObjList, epJSON);
            } else if (temp->dataSize>0) {
                size_t epJSONStrSize=strlen("{\"Season\":\"\",\"Episode\":\"\",\"Title\":\"\",\"File\":\"\",\"Subs\":[]}")+strlen(seNum)+strlen(epNum)+strlen(epName)+strlen(temp->data[0])+strlen(epSubs)+1;
                epJSON=NULL;
                mallocMacro(epJSON, epJSONStrSize, "findEpisodes error");
                epJSON[0]='\0';
                snprintf(epJSON, epJSONStrSize, "{\"Season\":\"%s\",\"Episode\":\"%s\",\"Title\":\"%s\",\"File\":\"%s\",\"Subs\":[%s]}", seNum, epNum, epName, temp->data[0], epSubs);
                addData(jsonObjList, epJSON);
            }
            tryFree(epJSON);
            tryFree(epSubs);
            tryFree(epName);
            tryFree(epNum);
            tryFree(seNum);
        }
        tryFree(tempStr);
        thisThread->list=jsonObjList;
    }
    freeList(foundFiles);
    return NULL;
}

char *getShowPoster(progConfig *conf, unsigned int tmdb_id) {
    char *imgURL=NULL;
    size_t posterURLSize=strlen(tmdbSite)+strlen(tmdbTV)+intSize(tmdb_id)+strlen(tmdbP)+strlen(conf->TMDBapi)+strlen(tmdbP_Opts)+strlen(conf->prefImgLangTV)+2;
    char *posterURL=NULL;
    mallocMacro(posterURL, posterURLSize, "getShowPoster error");
    posterURL[0]='\0';
    snprintf(posterURL, posterURLSize, "%s%s%u%s%s%s,%s", tmdbSite, tmdbTV, tmdb_id, tmdbP, conf->TMDBapi, tmdbP_Opts, conf->prefImgLangTV);
    if (posterURL!=NULL) {
        imgURL=getPoster(posterURL, conf, conf->prefImgWidthTV, conf->prefImgRatioTV, conf->prefImgLangTV);
        
        if (imgURL==NULL) {
            printError("getShowPoster error", true, HRED, "failed while using URL '%s';\nRetrying without language option...\n", posterURL);
            posterURL[0]='\0';
            snprintf(posterURL, posterURLSize, "%s%s%u%s%s", tmdbSite, tmdbTV, tmdb_id, tmdbP, conf->TMDBapi);
            imgURL=getPoster(posterURL, conf, conf->prefImgWidthTV, conf->prefImgRatioTV, NULL);
        }
        
        if (imgURL==NULL) {
            printError("getShowPoster error", true, HRED, "failed while using URL '%s';\n", posterURL);
        } else {
            printInfo("getShowPoster info", true, "got poster for \"%d\", URL: \"%s\";\n", tmdb_id, imgURL);
            if (imgURL!=NULL && conf->dTVImg) { // if image should actually be downloaded, otherwise will just link to imgURL
                if (checkFolder(conf->dTVFolder, true)==0) {
                    size_t dlFileStrLen=strlen(imgURL)+strlen(conf->dTVFolder);
                    char *dlFileName=NULL;
                    mallocMacro(dlFileName, dlFileStrLen, "getShowPoster error");
                    dlFileName[0]='\0';
                    snprintf(dlFileName, dlFileStrLen, "%s%s", conf->dTVFolder, strrchr(imgURL, '/')+1);
                    
                    if (dlFile(conf, imgURL, dlFileName)==CURLE_OK) { // downloaded poster!
                        imgURL=realloc(imgURL, dlFileStrLen+1);
                        if (imgURL==NULL)
                            fatalError_abort("getShowPoster error", "could not realloc;\nError: %s;\n", strerror(errno));
                        
                        strlcpy(imgURL, dlFileName, dlFileStrLen);
                        if (conf->compressImgTV) {
                            imgURL=compressImg(conf->compressImgTVCmd, imgURL, true);
                        }
                    } 
                    tryFree(dlFileName);
                }
            }
        }
    tryFree(posterURL);
    } else {
        printError("getShowPoster error", false, HRED, "could not build URL request string, something went wrong...\n");
    }
    return imgURL;
}

char *getEpisodeName(progConfig *conf, unsigned int showId, char *seNum, char *epNum, char *TMDBapi) {
    char *epName=NULL;
    char *season="/season/";
    char *episode="/episode/";
    char *apiOpts="?api_key=";
    size_t tempStrSize=strlen(tmdbSite)+strlen(tmdbTV)+intSize(showId)+strlen(season)+strlen(seNum)+strlen(episode)+strlen(epNum)+strlen(apiOpts)+strlen(TMDBapi)+1;
    char *tempStr=NULL;
    mallocMacro(tempStr, tempStrSize, "getEpisodeName error");
    tempStr[0]='\0';
    snprintf(tempStr, tempStrSize, "%s%s%u%s%s%s%s%s%s", tmdbSite, tmdbTV, showId, season, seNum, episode, epNum, apiOpts, TMDBapi);
    memBlock *mem=initBlock();
    printInfo("getEpisodeName info", true, "finding episode name for show: \"%d\", season:\"%s\", episode: \"%s\", URL: \"%s\";\n", showId, seNum, epNum, tempStr);
    getRequest(conf, tempStr, mem, curlMemCb);

    cJSON *json_root=cJSON_Parse(mem->memory);
    if (json_root!=NULL) {
        cJSON *json_name=cJSON_DetachItemFromObject(json_root, "name");
        if (json_name!=NULL){
            char *tempEpName=cJSON_Print(json_name);
            if (tempEpName!=NULL) {
                // remove quotation marks at the beginning and end of string
                size_t tempEpNameSize=strlen(tempEpName);
                epName=NULL;
                mallocMacro(epName, tempEpNameSize+1, "getEpisodeName error");
                strlcpy(epName, tempEpName+1, tempEpNameSize-1);
            } 
            tryFree(tempEpName);
        } else {
            if (cJSON_GetNumberValue(cJSON_GetObjectItem(json_root, "status_code"))!=34) {
                printError("getEpisodeName warning", true, HYEL, " json_name==NULL - JSON received was:\n");
                char *cJSONstr=cJSON_Print(json_root);
                printError("", true, HBLU, cJSONstr);
                printError("", true, HYEL, "\nrequest was: '%s%s%s';", HBLU, tempStr, HYEL);
                printError("", true, HYEL, "\nEND;\n%s", COLOR_RESET);
                tryFree(cJSONstr);
            }
        }
        cJSON_Delete(json_name);
    } else {
        printError("getEpisodeName warning", true, HYEL, " json_root==NULL\n");
    }
    tryFree(tempStr);
    cJSON_Delete(json_root);
    freeBlock(mem);
    char *fixedEpName=replaceAll(epName, "\\\"", "");
    tryFree(epName);
    return fixedEpName;
}

void createShowsHTML(progConfig *conf, fileList *list) {
    printInfo("createShowsHTML info", true, "building HTML for TV shows...\n");
    if (list==NULL || list->dataSize==0) {
        fatalError_abort("createShowsHTML error", "list was NULL\n");
    }
    if (checkFolder(conf->TVhtml, true)==-1) {
        fatalError_exit("createShowsHTML", "could not find \"%s\";\nExiting...\n", conf->TVhtml);
    }
    pthread_t *threads=NULL;
    mallocMacro(threads, sizeof(pthread_t)*list->listSize, "createShowsHTML error");
    threadStruct *threadObj=NULL;
    mallocMacro(threadObj, sizeof(threadStruct)*list->listSize, "createShowsHTML error");
    size_t i=0;
    for (fileList *temp=list; temp!=NULL; temp=temp->next) {
        threadObj[i].conf=conf;
        threadObj[i].data=temp->data;        
        threadObj[i].oldJSON=NULL;
        threadObj[i].list=newList();
        threadObj[i].id=i;

        pthread_create(&threads[i], NULL, showHTML, (void *) &threadObj[i]);
        i++;
    }

    i=0;
    fileList *htmlList=newList();
    addData(htmlList, TV_HTML_TOP);
    
    char *htmlStr=NULL;
    for (fileList *temp=list; temp!=NULL; temp=temp->next) {
        pthread_join(threads[i], NULL);
        char *showFile=NULL;
        char *showPoster=NULL;
        if (checkFolder(threadObj[i].list->data[0], false)==0) {
            showFile=getRelativePath(conf->TVhtml, threadObj[i].list->data[0]);
        } else {
            fatalError_exit("createShowsHTML", "could not find \"%s\";\nExiting...\n", threadObj[i].list->data[0]);
        }
        
        if (checkFolder(threadObj[i].list->data[1], false)==0) {
            showPoster=getRelativePath(conf->TVhtml, threadObj[i].list->data[1]);
        } else {
            fatalError_exit("createShowsHTML", "could not find \"%s\";\nExiting...\n", threadObj[i].list->data[1]);
        }

        if (showPoster==NULL) {
            showPoster=NULL;
            mallocMacro(showPoster, 1, "createShowsHTML error");
            showPoster[0]='\0';
        }
        char *showName=threadObj[i].list->data[2];

        size_t htmlStrSize=strlen(TV_HTML_FRAME)+strlen(showFile)+strlen(showPoster)+strlen(showName)*2+intSize(i)+1;
        htmlStr=realloc(htmlStr, htmlStrSize);
        if (htmlStr==NULL) {
            fatalError_abort("createShowsHTML error", "could not realloc;\nError: %s;\n", strerror(errno));
        }
        snprintf(htmlStr, htmlStrSize, TV_HTML_FRAME, i, showName, showFile, showPoster, showName, i, i, i);

        addData(htmlList, htmlStr);
        freeList(threadObj[i].list);
        tryFree(showFile);
        tryFree(showPoster);
        i++;
    }
    addData(htmlList, TV_HTML_BOT);
    fileListToFile(htmlList, conf->TVhtml, "", "");

    tryFree(threadObj);
    tryFree(threads);
    tryFree(htmlStr);
    freeList(htmlList);
}

void *showHTML(void *threadArg) {
    threadStruct *thisThread=threadArg;
    fileList *this_show=newList();
    cJSON *myJSON=cJSON_Parse(thisThread->data[0]);
    if (myJSON==NULL) {
        printError("showHTML warning", false, HYEL, " myJSON was NULL!\tThis show's JSON is  unparsable!\nJSON dump:\n%s\n",  thisThread->data[0]);
        return NULL;
    }
    cJSON *showObj=cJSON_GetObjectItem(myJSON, "Show");
    cJSON *posObj=cJSON_GetObjectItem(myJSON, "Poster");
    char *showPoster=NULL;
    char *showName=NULL;
    if (posObj!=NULL && cJSON_GetStringValue(posObj)!=NULL) {
        showPoster=cJSON_GetStringValue(posObj);
    }
    if (showObj!=NULL && cJSON_GetStringValue(showObj)!=NULL) {
        showName=cJSON_GetStringValue(showObj);
    }
    if (showPoster==NULL) {
        showPoster=genImage(thisThread->conf->AutogenImgResizeTVCmd, thisThread->conf->dTVFolder, showName);
        /*
        // useless on genImage? compressed results bigger than original
        if (thisThread->conf->compressImgTV) {
            showPoster=compressImg(thisThread->conf->compressImgTVCmd, showPoster, true);
        }
        */
    }
    if (showName==NULL) {
        fatalError_abort("showHTML error", "showName==NULL; JSON was:\n%s\n--- END ---\n", cJSON_Print(myJSON));
    }

    int uuid=thisThread->id;
    cJSON *episodesArray=cJSON_GetObjectItem(myJSON, "Episodes");
    cJSON *extrasArray=cJSON_GetObjectItem(myJSON, "Extras");
    if (episodesArray==NULL || extrasArray==NULL) {
        fatalError_abort("showHTML error", " episodesArray or extrasArray were equal to NULL; JSON was:\n%s\n--- END ---\n ", cJSON_Print(myJSON));
    }
    printInfo("showHTML info", true, "building HTML for \"%s\";\n", showName);
    cJSON *episode=NULL;

    addData(this_show, SHOW_HTML_TOP);
    int currSeason=0;
    int uuidEpisode=0;
    size_t tempStrSize=strlen(SHOW_HTML_SEL)+intSize(uuid)+1; //'<select>' HTML size+uuid size+NULL
    char *tempStr=NULL;
    mallocMacro(tempStr, tempStrSize, "showHTML error");
    tempStr[0]='\0';
    snprintf(tempStr, tempStrSize, SHOW_HTML_SEL, uuid);
    addData(this_show, tempStr);

    cJSON_ArrayForEach(episode, episodesArray) {
        int this_seasonNum=0;
        cJSON *jsonSeason=cJSON_GetObjectItem(episode, "Season");
        if (jsonSeason!=NULL) {
            this_seasonNum=parseStrToInt(cJSON_GetStringValue(jsonSeason));
        }
        if (this_seasonNum>currSeason) {
            currSeason=this_seasonNum;
            tempStrSize=strlen(SHOW_HTML_OPT_SEASON)+intSize(currSeason)+intSize(currSeason)+1;
            tempStr=realloc(tempStr, tempStrSize);
            if (tempStr==NULL) {
                fatalError_abort("showHTML error", "could not realloc;\nError: %s;\n", strerror(errno));
            }
            snprintf(tempStr, tempStrSize, SHOW_HTML_OPT_SEASON, currSeason, currSeason);
            addData(this_show, tempStr);
        }
    }
    if (extrasArray!=NULL && cJSON_GetArraySize(extrasArray)>0) {
        currSeason++;
        tempStrSize=strlen(SHOW_HTML_OPT_EXTRAS)+intSize(currSeason)+1;
        tempStr=realloc(tempStr, tempStrSize);
        if (tempStr==NULL) {
            fatalError_abort("showHTML error", "could not realloc;\nError: %s;\n", strerror(errno));
        }
        snprintf(tempStr, tempStrSize, SHOW_HTML_OPT_EXTRAS, currSeason);
        addData(this_show, tempStr);
    }
    addData(this_show, "\n</select>");
    currSeason=0;
       
    cJSON_ArrayForEach(episode, episodesArray) {
        episodeHTML(this_show, thisThread->conf, episode, &currSeason, &uuid, &uuidEpisode);
    }
    addData(this_show, "\n</ul>");
    
    if (extrasArray!=NULL && cJSON_GetArraySize(extrasArray)>0) {
        currSeason++;
        tempStrSize=intSize(uuid)+intSize(currSeason)+strlen(SHOW_HTML_UL)+1;
        tempStr=realloc(tempStr, tempStrSize);
        if (tempStr==NULL)
            fatalError_abort("showHTML error", "could not realloc;\nError: %s;\n", strerror(errno));
        
        snprintf(tempStr, tempStrSize, SHOW_HTML_UL, uuid, currSeason);
        addData(this_show, tempStr);

        cJSON *extra=NULL;
        cJSON_ArrayForEach(extra, extrasArray) {
            episodeHTML(this_show, thisThread->conf, extra, &currSeason, &uuid, &uuidEpisode);
        }
        addData(this_show, "\n</ul>");
    }

    tempStrSize=intSize(uuid)+strlen(SHOW_HTML_BOT)+1;
    tempStr=realloc(tempStr, tempStrSize);
    if (tempStr==NULL)
        fatalError_abort("showHTML error", "could not realloc;\nError: %s;\n", strerror(errno));
        
    snprintf(tempStr, tempStrSize, SHOW_HTML_BOT, uuid);
    addData(this_show, tempStr);
    
    char *fileName=replaceAll(showName, " ", "");
    tempStrSize=strlen(fileName)+strlen(thisThread->conf->showHTMLFolder)+16;
    tempStr=realloc(tempStr, tempStrSize);
    if (tempStr==NULL) {
        fatalError_abort("showHTML error", "could not realloc;\nError: %s;\n", strerror(errno));
    }
    if (thisThread->conf->showHTMLFolder[strlen(thisThread->conf->showHTMLFolder)-1]=='/') {
        snprintf(tempStr, tempStrSize, "%sTV%s.html", thisThread->conf->showHTMLFolder, fileName);
    } else {
        snprintf(tempStr, tempStrSize, "%s/TV%s.html", thisThread->conf->showHTMLFolder, fileName);
    }

    if (checkFolder(thisThread->conf->showHTMLFolder, true)==0) {
        fileListToFile(this_show, tempStr, NULL, NULL);
     
        addData(thisThread->list, tempStr);
        addData(thisThread->list, showPoster);
        addData(thisThread->list, fileName);
    }
    if (posObj==NULL || cJSON_GetStringValue(posObj)==NULL) {
        tryFree(showPoster);
    }

    tryFree(fileName);
    tryFree(tempStr);
    cJSON_Delete(myJSON);
    freeList(this_show);
    return NULL;
}

void episodeHTML(fileList *this_show, progConfig *conf, cJSON *episode, int *currSeason, int *uuid, int *uuidEpisode) {
    int this_seasonNum=0;
    cJSON *jsonSeason=cJSON_GetObjectItem(episode, "Season");
    if (jsonSeason!=NULL) {
        this_seasonNum=parseStrToInt(cJSON_GetStringValue(jsonSeason));
    }
    char *this_episodeName=cJSON_GetStringValue(cJSON_GetObjectItem(episode, "Title"));
    char *this_file=getRelativePath(conf->TVhtml, cJSON_GetStringValue(cJSON_GetObjectItem(episode, "File")));
    cJSON *this_subs=cJSON_GetObjectItem(episode, "Subs");
    size_t tempStrSize=0;
    char *tempStr=NULL;

    if (this_seasonNum>*currSeason) {
        if (*currSeason>0) { // new season, so close prev season list
            addData(this_show, "\n</ul>\n");
        }
        (*currSeason)=this_seasonNum;
        tempStrSize=intSize(*uuid)+intSize(*currSeason)+strlen(SHOW_HTML_UL)+1;
        tempStr=realloc(tempStr, tempStrSize);
        if (tempStr==NULL) {
            fatalError_abort("episodeHTML error", "could not realloc;\nError: %s;\n", strerror(errno));
        }
        snprintf(tempStr, tempStrSize, SHOW_HTML_UL, *uuid, *currSeason);
        addData(this_show, tempStr);
    }
    
    tempStrSize=intSize(*uuid)+intSize(*uuidEpisode)+strlen(this_episodeName)+strlen(SHOW_HTML_LI)+1;
    tempStr=realloc(tempStr, tempStrSize);
    if (tempStr==NULL) {
        fatalError_abort("episodeHTML error", "could not realloc;\nError: %s;\n", strerror(errno));
    }
    snprintf(tempStr, tempStrSize, SHOW_HTML_LI, *uuid, *uuidEpisode, this_episodeName);
    addData(this_show, tempStr);
    
    tempStrSize=intSize(*uuid)+intSize(*uuidEpisode)+strlen(SHOW_HTML_DIV)+1;
    tempStr=realloc(tempStr, tempStrSize);
    if (tempStr==NULL) {
        fatalError_abort("episodeHTML error", "could not realloc;\nError: %s;\n", strerror(errno));
    }
    snprintf(tempStr, tempStrSize, SHOW_HTML_DIV, *uuid, *uuidEpisode);
    addData(this_show, tempStr);
    
    tempStrSize=strlen(this_episodeName)+strlen(SHOW_HTML_SPAN)+1;
    tempStr=realloc(tempStr, tempStrSize);
    if (tempStr==NULL) {
        fatalError_abort("episodeHTML error", "could not realloc;\nError: %s;\n", strerror(errno));
    }
    snprintf(tempStr, tempStrSize, SHOW_HTML_SPAN, this_episodeName);
    addData(this_show, tempStr);

    tempStrSize=intSize(*uuid)+intSize(*uuidEpisode)+strlen(this_file)+strlen(SHOW_HTML_VIDEO)+1;
    tempStr=realloc(tempStr, tempStrSize);
    if (tempStr==NULL) {
        fatalError_abort("episodeHTML error", "could not realloc;\nError: %s;\n", strerror(errno));
    }
    snprintf(tempStr, tempStrSize, SHOW_HTML_VIDEO, *uuid, *uuidEpisode, this_file);
    addData(this_show, tempStr);

    cJSON *currSub=NULL;
    cJSON_ArrayForEach(currSub, this_subs) {
        char *subLang=cJSON_GetStringValue(cJSON_GetObjectItem(currSub, "lang"));
        char *subPath=getRelativePath(conf->TVhtml, cJSON_GetStringValue(cJSON_GetObjectItem(currSub, "subFile")));
        if (subLang!=NULL && subPath!=NULL) {
            tempStrSize=strlen(SHOW_HTML_SUBS)+strlen(subLang)+strlen(subPath)+1;
            tempStr=realloc(tempStr, tempStrSize);
            if (tempStr==NULL) {
                fatalError_abort("episodeHTML error", "could not realloc;\nError: %s;\n", strerror(errno));
            }
            snprintf(tempStr, tempStrSize, SHOW_HTML_SUBS, subPath, subLang);
            addData(this_show, tempStr);
        }
        tryFree(subPath);
    }
    addData(this_show, SHOW_HTML_CMD);
    (*uuidEpisode)++;
    tryFree(tempStr);
    tryFree(this_file);
}

void *cleanTV(void *threadArg) {
    threadStruct *thisThread=threadArg;
    progConfig *conf=thisThread->conf;
    
    if (conf->tvDB_exists==false || conf->JSON_tvDB==NULL) {
        printError("cleanTV warning", false, HYEL, "no TV database to clean!\n");
    } else {
        printInfo("cleanTV info", false, "cleaning \"%s\"...\n", conf->dbNameTV);
        cJSON *tempJSON=cJSON_Duplicate(conf->JSON_tvDB, true);
        int jsonArrSize=cJSON_GetArraySize(tempJSON);
        pthread_t *threads=NULL;
        mallocMacro(threads, sizeof(pthread_t)*jsonArrSize, "cleanTV error");
        threadStruct *threadObj=NULL;
        mallocMacro(threadObj, sizeof(threadStruct)*jsonArrSize, "cleanTV error");
        cJSON *show=NULL;

        int i=0;
        cJSON_ArrayForEach(show, tempJSON) {
            threadObj[i].conf=conf;
            threadObj[i].oldJSON=cJSON_Duplicate(show, true);

            pthread_create(&threads[i], NULL, cleanShow, (void *) &threadObj[i]);
            i++;
        }
        cJSON_Delete(tempJSON);
        int numThreads=i;
        cJSON *newJSON=cJSON_CreateArray();
        for (i=0; i<numThreads; i++) {
            pthread_join(threads[i], NULL);
            if (threadObj[i].oldJSON != NULL) {
                cJSON_AddItemToArray(newJSON, threadObj[i].oldJSON);
            }
        }
        thisThread->oldJSON=cJSON_Duplicate(newJSON, true);
        cJSON_Delete(newJSON);
        tryFree(threads);
        tryFree(threadObj);
        return NULL;
    }

    return NULL;
}

void *cleanShow(void *threadArg) {
    threadStruct *thisThread=threadArg;
    cJSON *show=thisThread->oldJSON;
    cJSON *episodesArray;
    cJSON *extrasArray;
    int episodesArraySize=0;
    int extrasArraySize=0;

    if (show!=NULL) {
        if (cJSON_GetObjectItem(show, "Episodes")!=NULL) {
            episodesArray=cJSON_Duplicate(cJSON_GetObjectItem(show, "Episodes"), true);
            cJSON *episode=NULL;
            int i=0, numDeleted=0;
            cJSON_ArrayForEach(episode, episodesArray) {
                if (episode!=NULL && cJSON_GetObjectItem(episode, "File")!=NULL) {
                    char *tempStr=cJSON_GetStringValue(cJSON_GetObjectItem(episode, "File"));
                    if (tempStr!=NULL) {
                        if (access(tempStr, F_OK)==0) {
                            cJSON *subsArray=cJSON_Duplicate(cJSON_GetObjectItem(episode, "Subs"), true);
                            int j=0, subsDeleted=0;
                            cJSON *sub=NULL;
                            cJSON_ArrayForEach(sub, subsArray) {
                                char *tempSubStr=cJSON_GetStringValue(cJSON_GetObjectItem(sub, "subFile")); 
                                if (tempSubStr!=NULL) {
                                    if (access(tempSubStr, F_OK)!=0) {
                                        cJSON *tempSubs=cJSON_GetObjectItem(cJSON_GetArrayItem(cJSON_GetObjectItem(show, "Episodes"), i-numDeleted), "Subs");
                                        cJSON_DeleteItemFromArray(tempSubs, j-subsDeleted);
                                        subsDeleted++;
                                    }
                                }
                                j++;
                            }
                            cJSON_Delete(subsArray);
                        } else {
                            cJSON_DeleteItemFromArray(cJSON_GetObjectItem(show, "Episodes"), i-numDeleted);
                            numDeleted++;
                        }
                    }
                }
                i++;
            }
            episodesArraySize=cJSON_GetArraySize(episodesArray);
            cJSON_Delete(episodesArray);
        }
        if (cJSON_GetObjectItem(show, "Extras")!=NULL) {
            extrasArray=cJSON_Duplicate(cJSON_GetObjectItem(show, "Extras"), true);
            cJSON *extra=NULL;
            int i=0, numDeleted=0;
            cJSON_ArrayForEach(extra, extrasArray) {
                if (extra!=NULL && cJSON_GetObjectItem(extra, "File")!=NULL) {
                    char *tempStr=cJSON_GetStringValue(cJSON_GetObjectItem(extra, "File"));
                    if (tempStr!=NULL) {
                        if (access(tempStr, F_OK)==0) {
                            cJSON *subsArray=cJSON_Duplicate(cJSON_GetObjectItem(extra, "Subs"), true);
                            int j=0, subsDeleted=0;
                            cJSON *sub=NULL;
                            cJSON_ArrayForEach(sub, subsArray) {
                                char *tempSubStr=cJSON_GetStringValue(cJSON_GetObjectItem(sub, "subFile")); 
                                if (tempSubStr!=NULL) {
                                    if (access(tempSubStr, F_OK)!=0) {
                                        cJSON *tempSubs=cJSON_GetObjectItem(cJSON_GetArrayItem(cJSON_GetObjectItem(show, "Extras"), i-numDeleted), "Subs");
                                        cJSON_DeleteItemFromArray(tempSubs, j-subsDeleted);
                                        subsDeleted++;
                                    }
                                }
                                j++;
                            }
                            cJSON_Delete(subsArray);
                        } else {
                            cJSON_DeleteItemFromArray(cJSON_GetObjectItem(show, "Extras"), i-numDeleted);
                            numDeleted++;
                        }
                    }
                }
                i++;
            }
            extrasArraySize=cJSON_GetArraySize(extrasArray);
            cJSON_Delete(extrasArray);
        }
        if ((episodesArraySize+extrasArraySize) == 0) {
            //TODO: parse "Show" tag for filename then delete HTML show file?
            thisThread->oldJSON = NULL;
        }
    }

    return NULL;
}

int getShowID(progConfig *conf, char *tvShowName) {
    int tmdbID=0;
    if (conf->getTVposter || conf->getEpisodeName) { // if poster needed -> find out tmdb_id
        CURL *curl=curl_easy_init();

        char *escapedTVShowName=curl_easy_escape(curl, tvShowName, 0);
        char *apiOpts="&api_key=";
        size_t tempURLStrSize=strlen(tmdbTV_ID)+strlen(escapedTVShowName)+strlen(apiOpts)+strlen(conf->TMDBapi)+1;
        char *tempURLStr=NULL;
        mallocMacro(tempURLStr, tempURLStrSize, "getShowID error");
        tempURLStr[0]='\0';
        snprintf(tempURLStr, tempURLStrSize, "%s%s%s%s", tmdbTV_ID, escapedTVShowName, apiOpts, conf->TMDBapi);
        printInfo("getShowID info", true, "Searching for TMDB ID for \"%s\", URL: \"%s\";\n", tvShowName, tempURLStr);
        tmdbID=getTmdbID(tempURLStr, conf);
        if (tmdbID==0) {
            printError("getShowID warning", false, HYEL, "beware, could not find any ID for \"%s\";\n", tvShowName);
            tmdbID=-1;
        }
        curl_free(escapedTVShowName);
        curl_easy_cleanup(curl);
    }
    return tmdbID;
}
