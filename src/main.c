#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <bsd/string.h> 
#include <unistd.h>
#include <errno.h>
#include <string.h>            
#include <libgen.h>
#include <pwd.h>
#include "conf.h"
#include "utils.h"
#include "iofiles.h"
#include "tvshow.h"
#include "movies.h"
#include "fileList.h"
#include "thread.h"
#include "main.h"

#define REPO_URL "https://github.com/farfalleflickan/cmyflix"
#define VERSION_STRING "0.24"

// GLOBAL VARIABLES
FILE *LOGFILE=NULL;
/* PRINT_MODE: 
   0=default mode, (warnings, minimal progress messages)
   1=verbose mode (add function in error/warnings, additional progress messages), 
   2=quiet mode (print only warnings/errors (no progress messages)), 
   3=quieter mode (print only errors (no progress messages)), 
   4=quietest mode (print nothing) */
unsigned int PRINT_MODE=0; 
pthread_mutex_t *mutex_stdout;
struct timeval timeProgStart;


void printHelp() {
    resetSTDColors();
    printf("Usage: cmyflix [options]\n\n");
    printf("Options:\n");
    printf("  --conf path/to/file\t\t\tspecifies configuration file\n");
    printf("  --log  path/to/file\t\t\tspecify log file for all output\n");
    printf("  --html path/to/folder\t\t\tspecify path to html resources\n");
    printf("  --fix  \"name\"\t\t\t\trun fix mode, requires a mode to be set (either -m/--movies or -t/--tvshows) and at least one fix mode option\n");
    printf("  \t\t--id \t 12345\t\tspecifies new id for fix mode\n");
    printf("  \t\t--name \t \"new name\"\tspecifies new name for fix mode\n");
    printf("  \t\t--poster path/to/file\tspecifies new poster for fix mode\n");
    printf("  \t\t--refresh\t\trefresh info of \"name\", so refreshes episode names in TV shows (also re-downloads poster if --poster \"\" is passed)\n");
    printf("  --version\t\t\t\tprint version\n");
    printf("  --gen-config\t\t\t\tprint default configuration to shell\n");
    printf("  --check-update\t\t\tcheck if program update is available\n");
    printf("  -c, --clean\t\t\t\tclean database, check if database entries correspond to existing files\n");
    printf("  -h, --help\t\t\t\tprints this help screen\n");
    printf("  -m, --movies\t\t\t\trun in movies mode\n");
    printf("  -q, --quiet\t\t\t\tsuppress shell output (-q suppress info & only prints warnings, -qq suppresses all output, except fatal errors, -qqq print nothing)\n");
    printf("  -t, --tvshows\t\t\t\trun in tv-shows mode\n");
    printf("  -v, --verbose\t\t\t\tenhance shell output\n");
    printf("  -D, --db\t\t\t\tbuild database\n");
    printf("  -H, --html\t\t\t\tbuild HTML files\n\n");
    printf("For documentation, see: " REPO_URL "\n");
}

void printVersion() {
    resetSTDColors();
    printf("cmyflix - version: %s\n", VERSION_STRING);
    printf("Created by Daria Rostirolla;\n");
    printf(REPO_URL "\n");
    exit(EXIT_SUCCESS);
}

// check github to see if there is a new version
void checkUpdate() {
    const char *URL="https://raw.githubusercontent.com/farfalleflickan/cmyflix/master/src/main.c";
    const char *getVersionCmd="curl -s %s | grep -i '#define VERSION_STRING ' | sed -e 's/#define VERSION_STRING\\| //g;s/\"//g'";
    size_t cmdStrSize=strlen(getVersionCmd)+strlen(URL)+1;
    char *cmdStr=NULL;
    mallocMacro(cmdStr, cmdStrSize, "checkUpdate error");
    snprintf(cmdStr, cmdStrSize, getVersionCmd, URL);

    resetSTDColors();
    FILE *cmdRet=popen(cmdStr, "r");
    if (cmdRet==NULL) {
        tryFree(cmdStr);
        fatalError_exit("checkUpdate", "something went wrong while trying to check for an update, errno is: %s\n", strerror(errno));
    } else {
        double foundVersion=-1;
        fscanf(cmdRet, "%lf", &foundVersion);
        if (foundVersion==-1) {
            fatalError_exit("checkUpdate", "something went wrong while trying to check for an update, used URL: %s\n", URL);
        } else if (foundVersion==parseStrToDouble(VERSION_STRING)) {
            fprintf(stdout, HYEL "cmyflix is up-to-date\n" COLOR_RESET);
        } else if (foundVersion>parseStrToDouble(VERSION_STRING)) {
            fprintf(stdout, HGRN "Found an update to version: %g;\nFor more info, see: %s\n" COLOR_RESET, foundVersion, REPO_URL);
        }
    }

    if (pclose(cmdRet)!=0) {
        printError("cmyflix", false, HRED, "%s\n", strerror(errno)); 
    }
    tryFree(cmdStr);
}

// tv shows code
void *tvCode(void *args) {
    threadStruct *threadArg=(threadStruct *) args;
    progConfig *conf=threadArg->conf; 
    progFlags runFlags=threadArg->flags; 
    if (runFlags & SHOWS_MODE) {
        fileList *tv=NULL;
        if (runFlags & DB_MODE) {
            tv=createTVShowDB(conf); // find files and create/edit database
            if (tv!=NULL) {
                fileListToFile(tv, conf->dbNameTV, "[", "]");
            } else {
                printError("cmyflix", false, HYEL, "nothing in db to save to \"%s\";\n", conf->dbNameTV);
            }
        }
        if (runFlags & HTML_MODE) {
            if (tv==NULL && (runFlags & DB_MODE)==0) {
                tv=JSONtofileList(conf->JSON_tvDB, TV_MODE);
                if (tv==NULL) {
                    printError("cmyflix warning", false, HYEL, "Running in HTML mode but no database could be found!\nBuilding new database...\n");
                    tv=createTVShowDB(conf); // find files and create/edit database
                    if (tv!=NULL) {
                        fileListToFile(tv, conf->dbNameTV, "[", "]");
                    } else {
                        printError("cmyflix", false, HYEL, "nothing in db to save to \"%s\";\n", conf->dbNameTV);
                    }
                }
            }
            if (tv!=NULL) {
                createShowsHTML(conf, tv);
            } else {
                printError("cmyflix", false, HYEL, "nothing in db to save to \"%s\";\n", conf->TVhtml);
            }
        }
        freeList(tv);
    }
    return NULL;
}

// movies code
void *movieCode(void *args) {
    threadStruct *threadArg=(threadStruct *) args;
    progConfig *conf=threadArg->conf; 
    progFlags runFlags=threadArg->flags; 

    if (runFlags & MOVIES_MODE) {
        fileList *movies=NULL;
        if (runFlags & DB_MODE) {
            movies=createMoviesDB(conf);
            if (movies!=NULL) {
                fileListToFile(movies, conf->dbNameMovie, "[", "]");
            } else {
                printError("cmyflix", false, HYEL, "nothing in db to save to \"%s\";\n", conf->dbNameMovie);
            }
        }
        if (runFlags & HTML_MODE) {
            if (movies==NULL && (runFlags & DB_MODE)==0) {
                movies=JSONtofileList(conf->JSON_moDB, MO_MODE);
                if (movies==NULL) {
                    movies=createMoviesDB(conf);
                    if (movies!=NULL) {
                        fileListToFile(movies, conf->dbNameMovie, "[", "]");
                    } else {
                        printError("cmyflix", false, HYEL, "nothing in db to save to \"%s\";\n", conf->dbNameMovie);
                    }
                }
            }
            if (movies!=NULL) {
                createMoviesHTML(conf, movies);
            } else {
                printError("cmyflix", false, BHYEL, "nothing in db to save to \"%s\";\n", conf->Mhtml);
            } 
        }
        freeList(movies);
    }
    return NULL;
}

void copyExtras(progConfig *conf, progFlags runFlags, char *htmlFolder, char *defExtPath[]) {
    char *extPath=NULL;
    // if "--html" option is passed
    if (htmlFolder!=NULL) {
        extPath=htmlFolder;
    } else { // otherwise load from found extras in defaults paths
        for (int i=0; extPath==NULL; i++) {
            if (defExtPath[i]!=NULL) { 
                if (access(defExtPath[i], F_OK)==0) {
                    extPath=defExtPath[i];
                }
            } else {
                extPath=NULL;
                break;
            }
        }
    }
    char *tempStr=NULL;
    if (extPath!=NULL) {
        if (runFlags & HTML_MODE) {
            if ((runFlags & MOVIES_MODE)) {
                tempStr=dirname(conf->Mhtml);
            }
            if ((runFlags & SHOWS_MODE)) {
                tempStr=dirname(conf->TVhtml);
            }
            if (tempStr!=NULL) {
                char *tempStr2=appendSlash(tempStr);
                if (checkFolder(tempStr2, true)==0) {
                    char *cmdStr=NULL;
                    size_t tempStrSize=strlen("cp -r /*  ")+strlen(extPath)+strlen(tempStr2)+1;
                    mallocMacro(cmdStr, tempStrSize, "cmyflix error");
                    snprintf(cmdStr, tempStrSize, "cp -r %s/* %s", extPath, tempStr2);
                    resetSTDColors();
                    FILE *cmdRet=popen(cmdStr, "r");
                    if (cmdRet==NULL) {
                        tryFree(cmdStr);
                        fatalError_exit("cmyflix error", "something went wrong while trying to copy HTML resources from '%s' to '%s';\n", extPath, tempStr2);
                   } else {
                       if (pclose(cmdRet)!=0) {
                           printError("cmyflix", false, HRED, "something went wrong while trying to copy HTML resources from '%s' to '%s';\n", extPath, tempStr2);
                           printError("cmyflix", false, HRED, "%s\n", strerror(errno));                       
                       } else {
                           printInfo("", false, "Copied HTML resources from '%s' to '%s';\n", extPath, tempStr2);
                       }
                   }
                   tryFree(cmdStr);
                }
                if (tempStr2!=tempStr) {
                    tryFree(tempStr2);
                }
            } else {
                printError("cmyflix error", false, HRED, "could not get path to copy HTML resources to.\n");
            }
        }
    } else {
        printError("cmyflix error", false, HRED, "could not find HTML resources, paths tested:\n");
        for (int i=0; defExtPath[i]!=NULL; i++) {
            printError("cmyflix error", false, HRED, "\t%s\n", defExtPath[i]);
        }
        printError("cmyflix error", false, HRED, "nothing copied;\n");
    }
}

void cleanMode(progConfig *conf, progFlags flags) {
    pthread_t tvThread, movieThread;
    threadStruct threadArgTV, threadArgMovies;
    threadArgTV.conf=conf;
    threadArgTV.flags=flags;
    threadArgTV.oldJSON=NULL;
    threadArgMovies.conf=conf;
    threadArgMovies.flags=flags;
    threadArgMovies.oldJSON=NULL;
    
    if (flags & SHOWS_MODE) {
        pthread_create(&tvThread, NULL, cleanTV, &threadArgTV);
    }
    if (flags & MOVIES_MODE) {
        pthread_create(&movieThread, NULL, cleanMovies, &threadArgMovies);
    }
    
    if (flags & SHOWS_MODE) {
        pthread_join(tvThread, NULL);
        if (threadArgTV.oldJSON!=NULL) {
            char *newDB=cJSON_Print(threadArgTV.oldJSON);
            cJSON_Delete(threadArgTV.oldJSON);
            freeFileMem(conf->dbNameTV, conf->tvDB_str);
            writeCharToFile(newDB, conf->dbNameTV);
            tryFree(newDB);
            cJSON_Delete(conf->JSON_tvDB);
            conf->JSON_tvDB=NULL;
            conf->tvDB_str=fileToMem(conf->dbNameTV);
            conf->JSON_tvDB=cJSON_Parse(conf->tvDB_str);
        } else {
            printInfo("cleanMode warning", true, "oldJSON was NULL!\n");
        }
    }
    if (flags & MOVIES_MODE) {
        pthread_join(movieThread, NULL);
        if (threadArgMovies.oldJSON!=NULL) {
            char *newDB=cJSON_Print(threadArgMovies.oldJSON);
            cJSON_Delete(threadArgMovies.oldJSON);
            freeFileMem(conf->dbNameMovie, conf->moDB_str);
            writeCharToFile(newDB, conf->dbNameMovie);
            tryFree(newDB);
            cJSON_Delete(conf->JSON_moDB);
            conf->JSON_moDB=NULL;
            conf->moDB_str=fileToMem(conf->dbNameMovie);
            conf->JSON_moDB=cJSON_Parse(conf->moDB_str);
        } else {
            printInfo("cleanMode warning", true, "oldJSON was NULL!\n");
        }
    }

    if (flags & HTML_MODE) {
        flags&=~DB_MODE; 
        threadArgTV.flags=flags;
        threadArgMovies.flags=flags;
        pthread_create(&tvThread, NULL, tvCode, &threadArgTV);
        pthread_create(&movieThread, NULL, movieCode, &threadArgTV);
        pthread_join(tvThread, NULL);
        pthread_join(movieThread, NULL);
    }
}

int main(int argc, char * argv[]) {
    gettimeofday(&timeProgStart, NULL);
    mallocMacro(mutex_stdout, sizeof(pthread_mutex_t), "cmyflix error");
    pthread_mutex_init(mutex_stdout, NULL);
    // construct default conf paths
    char *binaryPath=dirname(argv[0]);
    char *sameAsBinaryConfPath=NULL;
    char *sameAsBinaryExtPath=NULL;
    
    char *homeExtPath=NULL;
    struct passwd *userPW=getpwuid(getuid());
    size_t homeExtPathSize=strlen(userPW->pw_dir)+strlen("/.config/cmyflix/html/")+1;
    mallocMacro(homeExtPath, homeExtPathSize, "cmyflix error");
    snprintf(homeExtPath, homeExtPathSize, "%s/.config/cmyflix/html/", userPW->pw_dir);
    
    char *homeConfPath=NULL;
    size_t homeConfPathSize=strlen(userPW->pw_dir)+strlen("/.config/cmyflix/cmyflix.cfg")+1;
    mallocMacro(homeConfPath, homeConfPathSize, "cmyflix error");
    snprintf(homeConfPath, homeConfPathSize, "%s/.config/cmyflix/cmyflix.cfg", userPW->pw_dir);

    if (binaryPath!=NULL) {
        size_t tempStrSize=strlen(binaryPath)+strlen("/cmyflix.cfg")+1;
        mallocMacro(sameAsBinaryConfPath, tempStrSize, "cmyflix error");
        snprintf(sameAsBinaryConfPath, tempStrSize, "%s/cmyflix.cfg", binaryPath);
    }
    char *defConfPath[]={sameAsBinaryConfPath, homeConfPath, "/etc/cmyflix/cmyflix.cfg", NULL};
    if (binaryPath!=NULL) {
        size_t tempStrSize=strlen(binaryPath)+strlen("/cmyflix.cfg")+1;
        mallocMacro(sameAsBinaryExtPath, tempStrSize, "cmyflix error");
        snprintf(sameAsBinaryExtPath, tempStrSize, "%s/html/", binaryPath);
    }
    char *defExtPath[]={sameAsBinaryExtPath, homeExtPath, "/etc/cmyflix/html/", NULL};

    const char *short_options="chmqtvDH";
    struct option long_options[] = {
        {"conf",            required_argument, 0, '0'},
        {"log",             required_argument, 0, '1'},
        {"html",            required_argument, 0, '2'},
        {"fix",             required_argument, 0, '3'},
        {"id",              required_argument, 0, '4'},
        {"poster",          required_argument, 0, '5'},
        {"name",            required_argument, 0, '6'},
        {"refresh",         no_argument,       0, '7'},
        {"version",         no_argument,       0, '8'},
        {"gen-config",      no_argument,       0, '9'},
        {"check-update",    no_argument,       0, 'u'},
        {"clean",           no_argument,       0, 'c'},
        {"help",            no_argument,       0, 'h'},
        {"movies",          no_argument,       0, 'm'},
        {"quiet",           no_argument,       0, 'q'},
        {"tvshows",         no_argument,       0, 't'},
        {"verbose",         no_argument,       0, 'v'},
        {"db",              no_argument,       0, 'D'},
        {"html",            no_argument,       0, 'H'},
        {0,                 0,                 0,  0 }
    };

    progFlags runFlags=0;
    char *confFile=NULL, *htmlFolder=NULL, *fixStr=NULL, *fixID=NULL, *fixPoster=NULL, *fixName=NULL;
    progConfig *conf = NULL;
    opterr=0;
    
    for (int currOption=0; currOption!=-1; currOption=getopt_long(argc, argv, short_options, long_options, NULL)) {
        if (currOption=='0') { // --conf=
            size_t strLen=strlen(optarg)+1;
            if (confFile!=NULL) {
                tryFree(confFile);
            }
            confFile=NULL;
            mallocMacro(confFile, strLen, argv[0]);
            strlcpy(confFile, optarg, strLen);

            if (access(confFile, F_OK)==0) {
                conf=getConfig(confFile);
            } else {
                fatalError_exit(argv[0], "could not open configuration file \"%s\": %s\n", confFile, strerror(errno));
            }
        } else if (currOption=='1') { // --log=     
            LOGFILE=fopen(optarg, "w");
            if (LOGFILE==NULL) {
                fatalError_exit("", "could not use log \"%s\": %s\n", optarg, strerror(errno));
            }
        } else if (currOption=='2') { // --html=
            size_t strLen=strlen(optarg)+1;
            mallocMacro(htmlFolder, strLen, argv[0]);
            strlcpy(htmlFolder, optarg, strLen);

            if (access(htmlFolder, F_OK)==0) {
            } else {
                fatalError_exit(argv[0], "could not access HTML folder \"%s\": %s\n", htmlFolder, strerror(errno));
            }
        } else if (currOption=='3') { // --fix=
            runFlags |= FIX_MODE;
            fixStr=optarg;
        } else if (currOption=='4') { // --id
            runFlags |= FIX_ID_MODE;
            fixID=optarg;
        } else if (currOption=='5') { // --poster
            runFlags |= FIX_POSTER_MODE;
            fixPoster=optarg;
            if (optarg!=NULL && strlen(optarg)<=0) {
                fixPoster=NULL;
            } 
        } else if (currOption=='6') { // --name
            fixName=optarg;
            runFlags |= FIX_NAME_MODE;
        } else if (currOption=='7') { // --refresh
            runFlags |= FIX_REFR_MODE; 
        } else if (currOption=='8') { // --version
            printVersion();
            exit(EXIT_SUCCESS);
        } else if (currOption=='9') { // --gen-config
            printf("%s\n", DEF_CONF);
            exit(EXIT_SUCCESS);
        } else if (currOption=='c') { // --clean
            runFlags |= CLN_MODE;
        } else if (currOption=='u') { // --check-update
            checkUpdate();
            exit(EXIT_SUCCESS);
        } else if (currOption=='h') { // --help    -h
            printHelp();
            exit(EXIT_SUCCESS);
        } else if (currOption=='m') { // --movies  -m 
            runFlags |= MOVIES_MODE;
        } else if (currOption=='q') { // --quiet   -q
            if (PRINT_MODE<2) {
                PRINT_MODE=2;
            } else {
                PRINT_MODE++;
            }
        } else if (currOption=='t') { // --tvshows -t
            runFlags |= SHOWS_MODE;
        } else if (currOption=='v') { // --verbose -v
            PRINT_MODE=1;
        } else if (currOption=='D') { // --db      -D
            runFlags |= DB_MODE;
        } else if (currOption=='H') { // --html    -H
            runFlags |= HTML_MODE;
        } else if (currOption=='?') { // error handling
            if (optopt=='0' || optopt=='1' || optopt=='2' || optopt=='3' || optopt=='4' || optopt=='5' || optopt=='6') {
                int option_index=(char) optopt - '0';
                fatalError_exit(argv[0], "option '%s' requires an argument\n", long_options[option_index].name);
            }
            fatalError_exit(argv[0], "unknown argument\n");
        }
    }

    if (optind < argc) {
        printError(argv[0], false, HYEL, "invalid args: ");
        while (optind < argc) {
            fprintf(stderr, "%s ", argv[optind++]);
        }
        fprintf(stderr, "\n" COLOR_RESET);
        exit(EXIT_FAILURE);
    }

    if (runFlags==0) { // nothing specified, defaults to everything
        runFlags = SHOWS_MODE | MOVIES_MODE | DB_MODE | HTML_MODE;
    } else if (((runFlags & DB_MODE) || (runFlags & HTML_MODE)) && ((runFlags & SHOWS_MODE)==0 && (runFlags & MOVIES_MODE)==0) && (runFlags & FIX_MODE)==0) { 
        // if in either DB mode or HTML mode, but without movies/shows specified (will use both), NOT in fix mode
        runFlags |= SHOWS_MODE;
        runFlags |= MOVIES_MODE;
    } else if (((runFlags & DB_MODE)==0 && (runFlags & HTML_MODE)==0 && (runFlags & FIX_MODE)==0 && (runFlags & CLN_MODE)==0) && ((runFlags & SHOWS_MODE) || (runFlags & MOVIES_MODE))) {
        // if in either movie or show mode, but without DB or HTML specified (will use both), NOT in fix mode
        runFlags |= DB_MODE;
        runFlags |= HTML_MODE;
    } else if (((runFlags & DB_MODE) || (runFlags & HTML_MODE)) && ((runFlags & SHOWS_MODE) || (runFlags & MOVIES_MODE)) && (runFlags & FIX_MODE)==0) {
        // if in either DB or HTML mode AND either movies or shows mode, AND NOT in FIX_MODE
    } else if (runFlags == CLN_MODE) { // if only clean mode specified, add both shows & movies
        runFlags |= SHOWS_MODE;
        runFlags |= MOVIES_MODE;
    } else if (runFlags > FIX_MODE && (runFlags & FIX_MODE)==0) {
        printError("cmyflix", false, HRED, "Fix mode options passed without '--fix'\n");
        fatalError_exit(argv[0], "exiting...\n"); 
    } else if ((runFlags & FIX_MODE)==0 && (runFlags & CLN_MODE)==0) { // if nothing else was a match and not in FIX_MODE
        printError(argv[0], false, HYEL, "strange, unpredicted bit flags!\n");
        printBitFlags(runFlags);
        fatalError_exit(argv[0], "exiting...\n"); 
    }
    
    for (int i=0; conf==NULL; i++) {
        if (defConfPath[i]!=NULL) {
            if (access(defConfPath[i], F_OK)==0) {
                confFile=defConfPath[i];
                conf=getConfig(confFile);
            }
        } else {
            fatalError_exit(argv[0], "could not find any config file!\n");
        }
    }

    if (runFlags & FIX_MODE) { // if in fix mode
        // if in either TV show mode or movie mode *AND* ( --id or --poster or --name)
        if (((runFlags & SHOWS_MODE) || (runFlags & MOVIES_MODE)) && ((runFlags & FIX_ID_MODE) || (runFlags & FIX_POSTER_MODE) || (runFlags & FIX_NAME_MODE) || (runFlags & FIX_REFR_MODE))) {
            bool refreshMode=false;
            if (runFlags & FIX_REFR_MODE) {
                refreshMode=true;
            }

            if (fixMode(conf, runFlags, fixStr, fixID, fixPoster, fixName, refreshMode)!=0) { // fixMode found nothing to fix
                char *usedDB=NULL;
                if (runFlags & SHOWS_MODE) {
                    usedDB=conf->dbNameTV;
                } else if (runFlags & MOVIES_MODE) {
                    usedDB=conf->dbNameMovie;
                }
                printError(argv[0], false, HYEL, "could not find '%s' to fix in '%s'!\n", fixStr, usedDB);
            }  
        } else {
            printError(argv[0], false, HRED, "invalid options for --fix\n");
        }
    } else if (runFlags & CLN_MODE) { // if in clean mode
        cleanMode(conf, runFlags);
        copyExtras(conf, runFlags, htmlFolder, defExtPath);
    } else {
        pthread_t tvThread, movieThread;
        threadStruct threadArg;
        threadArg.conf=conf;
        threadArg.flags=runFlags;
        
        pthread_create(&tvThread, NULL, tvCode, &threadArg);
        pthread_create(&movieThread, NULL, movieCode, &threadArg);
        pthread_join(tvThread, NULL);
        pthread_join(movieThread, NULL);
        
        copyExtras(conf, runFlags, htmlFolder, defExtPath);
    }
    
    confCleanup(conf);
    pthread_mutex_destroy(mutex_stdout);
    tryFree(mutex_stdout);
    if (LOGFILE!=NULL) {
        fclose(LOGFILE);
    }
    tryFree(sameAsBinaryConfPath);
    tryFree(sameAsBinaryExtPath);
    tryFree(homeConfPath);
    tryFree(homeExtPath);
    tryFree(htmlFolder);
    return EXIT_SUCCESS;
}
