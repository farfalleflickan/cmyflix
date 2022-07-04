#include <cjson/cJSON.h>
#include <stdlib.h>
#include <pthread.h>
#include <bsd/string.h>
#include <errno.h>
#include <unistd.h>
#include "fileList.h"
#include "movies.h"
#include "iofiles.h"
#include "utils.h"
#include "thread.h"

struct fileList *createMoviesDB(progConfig *conf) {
    fileList *folders=NULL;
    char *infoStr="building home movies' database...";
    if (conf->homeMovies==false) {
        infoStr="building movies' database...";
        folders=find(conf, conf->MoviesPath, "", DI_MODE, false);
    }
    printInfo("createMoviesDB info", true, "%s\n", infoStr);


    if (folders!=NULL) { 
        int foldersListSize=folders->listSize, i=0;
        pthread_t *threads=NULL;
        mallocMacro(threads, sizeof(pthread_t)*foldersListSize, "createMoviesDB error");

        threadStruct *threadObj=NULL;
        mallocMacro(threadObj, sizeof(threadStruct)*foldersListSize, "createMoviesDB error");

        for (fileList *temp=folders; temp!=NULL; temp=temp->next, i++) {
            threadObj[i].conf=conf;
            threadObj[i].data=temp->data;
            threadObj[i].oldJSON=NULL;
            threadObj[i].list=NULL;
            threadObj[i].id=0;
            
            pthread_create(&threads[i], NULL, findMovies, (void *) &threadObj[i]);
        }

        fileList *movieJSON=NULL;
        for (i=0; i<foldersListSize; i++) { // loop through all found movies
            pthread_join(threads[i], NULL);
            if (threadObj[i].list!=NULL && threadObj[i].list->data!=NULL) { // if this list obj has data...
                movieJSON=joinLists(movieJSON, threadObj[i].list); // join all to single list
            } else {
                freeList(threadObj[i].list);
            }
        }
        tryFree(threads);
        tryFree(threadObj);
        freeList(folders);

        // sort movies alphabetically
        printInfo("createMoviesDB info", true, "sorting DB...\n");
        msortList(&movieJSON, 1, true, STRING);

        for (fileList *temp=movieJSON; temp!=NULL; temp=temp->next) { // remove temp->data[1] (which should be the movie name), meanwhile temp->data[0] is the whole movie JSON 
            if (temp->data!=NULL) {
                tryFree(temp->data[1]);
                temp->dataSize--;
            }
        }
        char *jsonStr=fileListToJSONStr(movieJSON);
        if (jsonStr!=NULL) {
            cJSON_Delete(conf->JSON_moDB);
            conf->JSON_moDB=cJSON_Parse(jsonStr);
        }
        tryFree(jsonStr);

        return movieJSON;
    } else { // basically, "home movies" behaviour
        folders=find(conf, conf->MoviesPath, videoExt, DI_MODE | FI_MODE, true);
        fileList *hmovieJSON=NULL;

        if (folders!=NULL) {
            if (conf->moDB_exists) { // Load old DB
                hmovieJSON=joinLists(hmovieJSON, JSONtofileList(conf->JSON_moDB, MO_MODE));
            }
            for (fileList *temp=folders; temp!=NULL; temp=temp->next) {
                if (temp->dataSize==3) { // stupid constant value
                    /* temp->data[0]=fullPath
                     * temp->data[1]="dirname"
                     * temp->data[2]=filename with extension
                     */
                    if (conf->moDB_exists==true && strstr(conf->moDB_str, temp->data[2])!=NULL) { // already in DB
                    } else { // Add to DB
                        fileList *thisFile=newList();
                        char *movieName=removeExtension(temp->data[2]);
                        char *imgURL=genImage(conf->AutogenImgResizeMoCmd, conf->dMoFolder, movieName);
                        /*
                        // useless on genImage? compressed results bigger than original
                        if (conf->compressImgMo) {
                        imgURL=compressImg(conf->compressImgMoCmd, imgURL, true);
                        }
                        */
                        if (imgURL==NULL) {
                            mallocMacro(imgURL, 1, "createMoviesDB error");
                            imgURL[0]='\0';
                        }
                        size_t JSONStrSize=strlen("{\"Movie\":\"\",\"ID\":\"\",\"Poster\":\"\",\"File\":\"\", \"Subs\":[]}")+strlen(movieName)+intSize(0)+strlen(imgURL)+strlen(temp->data[0])+1;
                        char *movieJSONStr=NULL;
                        mallocMacro(movieJSONStr, JSONStrSize, "createMoviesDB error");
                        movieJSONStr[0]='\0';
                        snprintf(movieJSONStr, JSONStrSize, "{\"Movie\":\"%s\",\"ID\":\"%d\",\"Poster\":\"%s\",\"File\":\"%s\",\"Subs\":[]}", movieName, 0, imgURL, temp->data[0]);
                        addData(thisFile, movieJSONStr);   
                        addData(thisFile, movieName);   
                        hmovieJSON=joinLists(hmovieJSON, thisFile);
                        tryFree(movieName);
                        tryFree(imgURL);
                        tryFree(movieJSONStr);
                    }

                } else {
                    printError("createMoviesDB error", false, HYEL, "weird dataSize! List was:\n");
                    printList(folders);
                    fatalError_abort("", "Panicing!\n");
                }
            }
            freeList(folders);
            // sort movies alphabetically
            printInfo("createMoviesDB info", true, "sorting DB...\n");
            msortList(&hmovieJSON, 1, true, STRING);

            for (fileList *temp=hmovieJSON; temp!=NULL; temp=temp->next) { // remove temp->data[1] (which should be the movie name), meanwhile temp->data[0] is the whole movie JSON 
                if (temp->dataSize>1) {
                    tryFree(temp->data[1]);
                    temp->dataSize--;
                }
            }
            char *jsonStr=fileListToJSONStr(hmovieJSON);
            if (jsonStr!=NULL) {
                cJSON_Delete(conf->JSON_moDB);
                conf->JSON_moDB=cJSON_Parse(jsonStr);
            }
            tryFree(jsonStr);

            return hmovieJSON;
        } else {
            printError("createMoviesDB warning", false, HYEL, "could not find anything in \"%s\";\n", conf->MoviesPath);
        }
    }
    printInfo("createMoviesDB info", true, "finished building DB\n");

    return NULL;
}

void *findMovies(void *threadArg) {
    threadStruct *thisThread=(threadStruct *) threadArg;
    progConfig *conf=thisThread->conf;

    size_t lenStr1=strlen(thisThread->data[0]);
    size_t lenStr2=strlen(thisThread->data[1]);
    char *fullPath=NULL;
    mallocMacro(fullPath, lenStr1+lenStr2+3, "findMovies error");
    strlcpy(fullPath, thisThread->data[0], lenStr1+3);
    if (fullPath[lenStr1 - 1] != '/') {
        strlcat(fullPath, "/",  lenStr1+lenStr2+3);
    }
    strlcat(fullPath, thisThread->data[1], lenStr1+lenStr2+3);
    if (thisThread->data[1][lenStr2-1] != '/') {
        strlcat(fullPath, "/", lenStr1+lenStr2+3);
    }
    // Find movie files 
    fileList *movies = find(conf, fullPath, videoExt, DI_MODE | FI_MODE, true); 
    thisThread->list=newList();

    if (movies!=NULL) { // no sub folders?
        // technically if folder contains just 1 movie, movies contains just 1 element
        // if folder is a "collection" then as many elements as movies
        for (fileList *temp=movies; temp!=NULL; temp=temp->next) {
            char *moviePath=temp->data[temp->dataSize-2]; // path to movie file
            char *movieFile=temp->data[0];
            char *movieNameNoExt=removeExtension(temp->data[temp->dataSize-1]); // name of movie file
            char *movieName=replaceAll(movieNameNoExt, ".", " ");
            char *imgURL=NULL;
            int movieID=0;
            
            // open JSON database/set up cJSON
            cJSON *tempJSON=NULL;
            if (conf->moDB_exists==true && strstr(conf->moDB_str, movieFile)!=NULL) {
                tempJSON=cJSON_Duplicate(conf->JSON_moDB, true);
                if (tempJSON!=NULL) {
                    cJSON *item=NULL;

                    cJSON_ArrayForEach(item, tempJSON) {
                        if (cJSON_GetObjectItem(item, "File")!=NULL && cJSON_GetStringValue(cJSON_GetObjectItem(item, "File"))!=NULL) {
                            if (strcmp(cJSON_GetStringValue(cJSON_GetObjectItem(item, "File")), movieFile)==0) { // match
                                movieID=parseStrToInt(cJSON_GetStringValue(cJSON_GetObjectItem(item, "ID")));
                                char *tempStr=cJSON_GetStringValue(cJSON_GetObjectItem(item, "Poster"));
                                size_t tempStrLen=strlen(tempStr)+1;
                                if (tempStrLen>3) {
                                    mallocMacro(imgURL, tempStrLen, "findMovies error");
                                    strlcpy(imgURL, tempStr, tempStrLen);
                                }
                                break;
                            }
                        }
                    }
                    cJSON_Delete(tempJSON);
                }
            } 

            char *movieSubs=NULL;
            if (conf->createMsubs) {
                movieSubs=getSubs(conf, movieNameNoExt, moviePath); // JSON of movie's subs
            }
            if (movieID==0) {
                movieID=getMovieID(conf, movieName);
            }

            if (imgURL==NULL && conf->getMposter==true && movieID>0) { 
                imgURL=getMoviePoster(thisThread->conf, movieID);
            } 

            if (imgURL==NULL) { // if still NULL, getMoviePoster didn't work... 
                imgURL=genImage(conf->AutogenImgResizeMoCmd, conf->dMoFolder, movieName);
                /*
                // useless on genImage? compressed results seem to be bigger than original
                if (conf->compressImgMo) {
                imgURL=compressImg(conf->compressImgMoCmd, imgURL, true);
                }
                */
            }

            if (imgURL==NULL) {
                mallocMacro(imgURL, 1, "findMovies error");
                imgURL[0]='\0';
            }
            if (movieSubs==NULL) {
                mallocMacro(movieSubs, 1, "findMovies error");
                movieSubs[0]='\0';
            }
            printInfo("findMovies info", true, "assembling JSON for \"%s\";\n", movieName);

            size_t movieJSONStrSize=strlen("{\"Movie\":\"\",\"ID\":\"\",\"Poster\":\"\",\"File\":\"\", \"Subs\":[]}")+strlen(movieName)+intSize(movieID)+strlen(imgURL)+strlen(movieFile)+strlen(movieSubs)+1;
            char *movieJSONStr=NULL;
            mallocMacro(movieJSONStr, movieJSONStrSize, "findMovies error");
            movieJSONStr[0]='\0';
            snprintf(movieJSONStr, movieJSONStrSize, "{\"Movie\":\"%s\",\"ID\":\"%d\",\"Poster\":\"%s\",\"File\":\"%s\",\"Subs\":[%s]}", movieName, movieID, imgURL, movieFile, movieSubs);

            addData(thisThread->list, movieJSONStr);
            addData(thisThread->list, movieName);
            if (temp->next!=NULL) {
                thisThread->list=add(thisThread->list, NULL, 0);
            }

            tryFree(movieNameNoExt);
            tryFree(movieName);
            tryFree(movieSubs);
            tryFree(imgURL);
            tryFree(movieJSONStr);
        }
    } else {
        printError("findMovies warning", false, HYEL, "please note, path \"%s\" is empty!\n", fullPath);
        freeList(thisThread->list);
    }
    freeList(movies);
    tryFree(fullPath);

    return NULL;
}

//fetch movie ID
int getMovieID(progConfig *conf, char *movieName) {
    int tmdbID=0;
    if (conf->getMposter==true) {
        CURL *curl=curl_easy_init();

        char *escapedMovieName=curl_easy_escape(curl, movieName, 0);
        char *apiOpts="&query=";
        size_t tempURLStrSize=strlen(tmdbM_ID)+strlen(conf->TMDBapi)+strlen(apiOpts)+strlen(escapedMovieName)+1;
        char *tempURLStr=NULL;
        mallocMacro(tempURLStr, tempURLStrSize, "getMovieID error");
        tempURLStr[0]='\0';
        snprintf(tempURLStr, tempURLStrSize, "%s%s%s%s", tmdbM_ID, conf->TMDBapi, apiOpts, escapedMovieName);
        if (tempURLStr!=NULL) {
            printInfo("getMovieID info", true, "Searching for TMDB ID for \"%s\", URL: \"%s\";\n", movieName, tempURLStr);
            tmdbID=getTmdbID(tempURLStr, conf);
            if (tmdbID==0) {
                printError("getMovieID warning", false, HYEL, "beware, could not find any ID for \"%s\";\n", movieName);
            }
            curl_free(escapedMovieName);
            curl_easy_cleanup(curl);
        } else {
            printError("getMovieID error", false, HRED, "could not build URL request string, something went wrong...\n");
        }
    }

    return tmdbID;
}

//fetch movie poster
char *getMoviePoster(progConfig *conf, int tmdb_id) {
    char *imgURL=NULL;
    size_t posterURLSize=strlen(tmdbSite)+strlen(tmdbM)+intSize(tmdb_id)+strlen(tmdbP)+strlen(conf->TMDBapi)+strlen(tmdbP_Opts)+strlen(conf->prefImgLangM)+2;
    char *posterURL=NULL;
    mallocMacro(posterURL, posterURLSize, "getMoviePoster error");
    posterURL[0]='\0';
    snprintf(posterURL, posterURLSize, "%s%s%d%s%s%s,%s", tmdbSite, tmdbM, tmdb_id, tmdbP, conf->TMDBapi, tmdbP_Opts, conf->prefImgLangM);
    if (posterURL!=NULL) {
        imgURL=getPoster(posterURL, conf, conf->prefImgWidthM, conf->prefImgRatioM, conf->prefImgLangM);
        if (imgURL==NULL) {
            printError("getMoviePoster error", true, HRED, "failed while using URL '%s';\n", posterURL);
        } else {
            printInfo("getMoviePoster info", true, "got poster for \"%d\", URL: \"%s\";\n", tmdb_id, imgURL);
            
            if (conf->dMoImg) { // if image should actually be downloaded, otherwise will just link to imgURL
                if (checkFolder(conf->dMoFolder, true)==0) {
                    size_t dlFileStrLen=strlen(imgURL)+strlen(conf->dMoFolder);
                    char *dlFileName=NULL;
                    mallocMacro(dlFileName, dlFileStrLen, "getMoviePoster error");
                    dlFileName[0]='\0';
                    snprintf(dlFileName, dlFileStrLen, "%s%s", conf->dMoFolder, strrchr(imgURL, '/')+1);

                    if (dlFile(conf, imgURL, dlFileName)==CURLE_OK) { // downloaded poster!
                        imgURL=realloc(imgURL, dlFileStrLen+1);
                        if (imgURL==NULL) {
                            fatalError_abort("getMoviePoster error", "could not realloc;\nError: %s;\n", strerror(errno));
                        }
                        strlcpy(imgURL, dlFileName, dlFileStrLen);
                        if (conf->compressImgMo) { // compress image
                            imgURL=compressImg(conf->compressImgMoCmd, imgURL, true);
                        }
                    } 
                    tryFree(dlFileName);
                }
            }
        }
        tryFree(posterURL);
    } else {
        printError("getMoviePoster error", false, HRED, "could not build URL request string, something went wrong...\n");
    }
    return imgURL;
}

// create HTML for movies, spawns a thread for every movie object in JSON calling movieHTML
void createMoviesHTML(progConfig *conf, fileList *list) {
    printInfo("createMoviesHTML info", true, "building HTML for movies...\n");
    if (list==NULL || list->listSize<1) {
        printError("createMoviesHTML warning", false, HYEL, "list was empty!\n");
        return;
    }
    pthread_t *threads=NULL;
    mallocMacro(threads, sizeof(pthread_t)*list->listSize, "createMoviesHTML error");
    threadStruct *threadObj=NULL;
    mallocMacro(threadObj, sizeof(threadStruct)*list->listSize, "createMoviesHTML error");
    int i=0;
    for (fileList *temp=list; temp!=NULL; temp=temp->next, i++) {
        threadObj[i].conf=conf;
        threadObj[i].data=temp->data;        
        threadObj[i].oldJSON=NULL;
        threadObj[i].list=newList();
        threadObj[i].id=i;

        pthread_create(&threads[i], NULL, movieHTML, (void *) &threadObj[i]);
    }

    i=0;
    fileList *htmlList=newList();
    addData(htmlList, MOVIE_HTML_TOP);

    for (fileList *temp=list; temp!=NULL; temp=temp->next, i++) {
        pthread_join(threads[i], NULL);
        for (size_t j=0; j<threadObj[i].list->dataSize; j++) {
            addData(htmlList, threadObj[i].list->data[j]);
        }

        freeList(threadObj[i].list);
    }
    addData(htmlList, MOVIE_HTML_BOT);

    fileListToFile(htmlList, conf->Mhtml, "", "");

    tryFree(threadObj);
    tryFree(threads);
    freeList(htmlList);
}

void *movieHTML(void *threadArg) {
    threadStruct *thisThread=threadArg;
    progConfig *conf=thisThread->conf;

    cJSON *myJSON=cJSON_Parse(thisThread->data[0]);
    if (myJSON==NULL) {
        printError("movieHTML warning", true, HYEL, " myJSON was NULL!\tThis show's JSON is  unparsable!\nJSON dump:\n%s\n",  thisThread->data[0]);
        return NULL;
    }
    cJSON *movObj=cJSON_GetObjectItem(myJSON, "Movie");
    cJSON *posObj=cJSON_GetObjectItem(myJSON, "Poster");
    cJSON *fileObj=cJSON_GetObjectItem(myJSON, "File");
    cJSON *mySubs=cJSON_GetObjectItem(myJSON, "Subs");
    char *moviePoster=NULL;
    char *movieName=NULL;
    char *movieFile=NULL;
    if (movObj!=NULL && cJSON_GetStringValue(movObj)!=NULL) {
        movieName=cJSON_GetStringValue(movObj);
    }
    if (posObj!=NULL && cJSON_GetStringValue(posObj)!=NULL && strlen(cJSON_GetStringValue(posObj))>1) {
        if (checkFolder(cJSON_GetStringValue(posObj), false)==0) {
            moviePoster=getRelativePath(conf->Mhtml, cJSON_GetStringValue(posObj));
        } else {
            fatalError_exit("movieHTML", "could not find \"%s\";\nExiting...\n", cJSON_GetStringValue(posObj));
        }
    }
    if (fileObj!=NULL && cJSON_GetStringValue(fileObj)!=NULL) {
        movieFile=getRelativePath(conf->Mhtml, cJSON_GetStringValue(fileObj));
    }
    if (moviePoster==NULL && movieName!=NULL) {
        char *genPath=genImage(thisThread->conf->AutogenImgResizeMoCmd, thisThread->conf->dMoFolder, movieName);
        moviePoster=getRelativePath(conf->Mhtml, genPath);
        tryFree(genPath);
        /*
        // useless on genImage? compressed results bigger than original
        if (conf->compressImgMo) {
        moviePoster=compressImg(conf->compressImgMoCmd, moviePoster, true);
        }
        */
    }
    if (movieName==NULL) {
        fatalError_abort("movieHTML error", "movieName==NULL; JSON was:\n%s\n--- END ---\n", cJSON_Print(myJSON));
    }
    if (movieFile==NULL) {
        fatalError_abort("movieHTML error", "movieFile==NULL; JSON was:\n%s\n--- END ---\n", cJSON_Print(myJSON));
    }
    printInfo("movieHTML info", true, "building HTML for \"%s\";\n", movieName);

    size_t tempStrSize=strlen(MOVIE_HTML_VIDEO)+intSize(thisThread->id)*3+strlen(moviePoster)+strlen(movieFile)+strlen(movieName)*2+1;
    char *tempStr=NULL;
    mallocMacro(tempStr, tempStrSize, "movieHTML error");

    snprintf(tempStr, tempStrSize, MOVIE_HTML_VIDEO, thisThread->id, movieName, moviePoster, movieName, thisThread->id, thisThread->id, movieFile); 
    addData(thisThread->list, tempStr);

    cJSON *currSub=NULL;
    cJSON_ArrayForEach(currSub, mySubs) {
        char *subLang=cJSON_GetStringValue(cJSON_GetObjectItem(currSub, "lang"));
        char *subPath=getRelativePath(conf->Mhtml, cJSON_GetStringValue(cJSON_GetObjectItem(currSub, "subFile")));
        if (subLang!=NULL && subPath!=NULL) {
            tempStrSize=strlen(MOVIE_HTML_SUBS)+strlen(subLang)+strlen(subPath)+1;
            tempStr=realloc(tempStr, tempStrSize);
            if (tempStr==NULL) {
                fatalError_abort("episodeHTML error", "could not realloc;\nError: %s;\n", strerror(errno));
            }
            snprintf(tempStr, tempStrSize, MOVIE_HTML_SUBS, subPath, subLang);
            addData(thisThread->list, tempStr);
        }
        tryFree(subPath);
    }
    addData(thisThread->list, MOVIE_HTML_VIDEO_BOT);
    tryFree(moviePoster);
    tryFree(movieFile);
    tryFree(tempStr);

    cJSON_Delete(myJSON);
    return NULL; 
}

void *cleanMovies(void *threadArg) {
    threadStruct *thisThread=threadArg;
    progConfig *conf=thisThread->conf;
    
    if (conf->moDB_str==false || conf->JSON_moDB==NULL) {
        printError("cleanMovies warning", false, HYEL, "no movies database to clean!\n");
    } else {
        printInfo("cleanMovies info", false, "cleaning \"%s\"...\n", conf->dbNameMovie);
        cJSON *moviesRef=cJSON_Duplicate(conf->JSON_moDB, true);
        cJSON *moviesArray=cJSON_Duplicate(conf->JSON_moDB, true);
        cJSON *movie=NULL;
        int i=0, numDeleted=0;
        cJSON_ArrayForEach(movie, moviesArray) {
            if (movie!=NULL && cJSON_GetObjectItem(movie, "File")!=NULL) {
                char *tempStr=cJSON_GetStringValue(cJSON_GetObjectItem(movie, "File"));
                if (tempStr!=NULL) {
                    if (access(tempStr, F_OK)==0) {
                        cJSON *subsArray=cJSON_Duplicate(cJSON_GetObjectItem(movie, "Subs"), true);
                        int j=0, subsDeleted=0;
                        cJSON *sub=NULL;
                        cJSON_ArrayForEach(sub, subsArray) {
                            char *tempSubStr=cJSON_GetStringValue(cJSON_GetObjectItem(sub, "subFile")); 
                            if (tempSubStr!=NULL) {
                                if (access(tempSubStr, F_OK)!=0) {
                                    cJSON_DeleteItemFromArray(cJSON_GetObjectItem(cJSON_GetArrayItem(moviesRef, i-numDeleted), "Subs"), j-subsDeleted);
                                    subsDeleted++;
                                }
                            }
                            j++;
                        }
                        cJSON_Delete(subsArray);
                    } else {
                        cJSON_DeleteItemFromArray(moviesRef, i-numDeleted);
                        numDeleted++;
                    }
                }
            }
            i++;
        }
        cJSON_Delete(moviesArray);
        thisThread->oldJSON=moviesRef;
    }

    return NULL;
}
