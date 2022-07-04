#pragma once
#include <curl/curl.h>
#include <sys/resource.h> 
#include <cjson/cJSON.h>
#include <stdbool.h>
#include <string.h>
#include <pthread.h>

// default config
#define DEF_CONF "TVpath=\"/etc/cmyflix/TV/\"; # REMEMBER / at the end of path! identifies the path to the folder containing tv shows\nMoviesPath=\"/etc/cmyflix/Movies/\"; # identifies the path to the folder containing movies\n\ndbNameMovie=\"/etc/cmyflix/dbM.json\"; # identifies path and name of the movie database\ndbNameTV=\"/etc/cmyflix/dbTV.json\"; # identifies path and name of the tv show database\n\n# regex used to identify tv shows, uses the sS1eE01 or sS01eE01 or 01x01 or 1x01 format, in that order\nregexTVnum=\"2\";\nregexTVstr1=\"(.*)[.][sS]([0-9]{1,2})[eE]([0-9]{1,3})[.\\-\\_](.*)\";\nregexTVstr2=\"(.*)[.]([0-9]{1,2})[x]([0-9]{1,3})[.\\-\\_](.*)\";\n# name of extras folder used to identify tv shows extras, default is \"Season.Extra\"\nTVextraStr=\"Season.Extra\"; \nregexTVgroups=\"5\"; # expected number of capture groups of regex\n\nregexM=\"(.*)[.](.*)\"; # movie regex\n\nregexHM=\"([0-9]+\\-[0-9]+\\-[0-9]+)\"; # home movie regex\n\nTVhtml=\"/etc/cmyflix/output/TV.html\"; # path to the TV html page\nshowHTMLFolder=\"/etc/cmyflix/output/shows/\"; # path to where to put html of every tv show\nMhtml=\"/etc/cmyflix/output/Movies.html\"; # path to the movie html page\n\ndTVImg=\"true\"; # download TV poster images true/false\ndTVFolder=\"/etc/cmyflix/output/TVimg/\"; # path to folder containing the images once downloaded\nAutogenImgResizeTVCmd=\"convert -background purple -fill white -size 500x735 -pointsize 36 -gravity center caption:\";\ncompressImgTVCmd=\"convert -resize 500x -gravity center -crop 500x735+0+0 -strip -interlace Plane -gaussian-blur 0.05 -quality 80%\"; #might need some tweaking?\ncompressImgTV=\"false\"; # wether the image should be comprossed\nprefImgWidthTV=\"2000\"; # preferred width of poster image\nprefImgRatioTV=\"0.667\"; # preferred width/height ratio of poster image\nprefImgLangTV=\"en\"; # preferred language for poster image (ISO 639-1 code)\n\n# exactly the same as above\ndMoImg=\"true\";\ndMoFolder=\"/etc/cmyflix/output/MoImg/\";\nAutogenImgResizeMoCmd=\"convert -background purple -fill white -size 500x735 -pointsize 36 -gravity center caption:\";\ncompressImgMoCmd=\"convert -resize 500x -gravity center -crop 500x735+0+0 -strip -interlace Plane -gaussian-blur 0.05 -quality 80%\"; #might need some tweaking?\ncompressImgMo=\"false\";\nprefImgWidthM=\"2000\";\nprefImgRatioM=\"0.667\";\nprefImgLangM=\"en\"; # preferred language for poster image (ISO 639-1 code)\n\ngetTVposter=\"true\"; # download tv show metadata poster\ngetEpisodeName=\"true\"; # download every episode name\ncreateTVsubs=\"true\"; # search for subs in same folder\n\ngetMposter=\"true\"; # same as above\ncreateMsubs=\"true\"; # search for subs for movies\nhomeMovies=\"false\"; # $MoviesPath contains home movies, do NOT treat as actual movies. homeMovies are sorted by filename \n\nfileLimit=\"8192\"; # sets rlimit for max number of open files, 8192 seems a good value\n\n# your TMDB api key\nTMDBapi=\"\";"

typedef struct progConfig {
		char *TVpath;
		char *MoviesPath;
		char *dbNameMovie;
		char *dbNameTV;
		char **regexTVstr;
		char *TVextraStr;
        char *regexM;
		char *regexHM;
		char *TVhtml;
		char *showHTMLFolder;
        char *Mhtml;

		bool dTVImg;
		char *dTVFolder;
		char *AutogenImgResizeTVCmd;
        char *compressImgTVCmd;
		bool compressImgTV;
        int prefImgWidthTV;
        double prefImgRatioTV;
        char *prefImgLangTV;

		bool dMoImg;
		char *dMoFolder;
		char *AutogenImgResizeMoCmd;
        char *compressImgMoCmd;
		bool compressImgMo;
        int prefImgWidthM;
        double prefImgRatioM;
        char *prefImgLangM;

		bool getTVposter;
		bool getEpisodeName;
		bool createTVsubs;
		bool getMposter;
		bool createMsubs;
		bool homeMovies;

		char *TMDBapi;

		int regexTVnum;
		int regexTVgroups;
		int properties;
        size_t fileLimit;

        bool tvDB_exists;
        bool moDB_exists;
        char *tvDB_str;
        char *moDB_str;
        cJSON *JSON_tvDB;
        cJSON *JSON_moDB;
        
        CURLSH *cURLshare;
        pthread_mutex_t *cURLconnLock;

        struct rlimit defLim;
        struct rlimit newLim;
} progConfig;

progConfig *getConfig(char *srcPath);
void confCleanup(progConfig *options);
void initAll(progConfig *options);
