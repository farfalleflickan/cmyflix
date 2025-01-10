#pragma once
#include <curl/curl.h>
#include <stdint.h>
#include "conf.h"
#include "main.h"

#define tmdbSite "https://api.themoviedb.org/3/"
#define tmdbP "/images?api_key="
#define tmdbP_Opts "&include_image_language=null"
#define tmdbM "movie/"
#define tmdbM_ID "https://api.themoviedb.org/3/search/movie?page=1&api_key=" // blah&query=name +opts
#define tmdbM_Opts "&include_adult=false&language=en-US"
#define tmdbTV "tv/"
#define tmdbTV_ID "https://api.themoviedb.org/3/search/tv?page=1&query=" // name&api_key=blah +opts
#define tmdbTV_Opts "&language=en" //to find the ID based on name
#define tmdbImg "https://image.tmdb.org/t/p/original"
#define videoExt (char *[]){"mp4", "mkv", "ogv", "webm", NULL}
#define subExt1 "srt"
#define subExt2 "vtt"

#define mallocMacro(var, size, caller) do {var=malloc(size); if(var==NULL){fatalError_abort(caller, "could not malloc;\nError: %s;\n", strerror(errno));}}while(0)

//Regular text
#define BLK "\033[0;30m"
#define RED "\033[0;31m"
#define GRN "\033[0;32m"
#define YEL "\033[0;33m"
#define BLU "\033[0;34m"
#define MAG "\033[0;35m"
#define CYN "\033[0;36m"
#define WHT "\033[0;37m"

//Regular bold text
#define BBLK "\033[1;30m"
#define BRED "\033[1;31m"
#define BGRN "\033[1;32m"
#define BYEL "\033[1;33m"
#define BBLU "\033[1;34m"
#define BMAG "\033[1;35m"
#define BCYN "\033[1;36m"
#define BWHT "\033[1;37m"

//Regular underline text
#define UBLK "\033[4;30m"
#define URED "\033[4;31m"
#define UGRN "\033[4;32m"
#define UYEL "\033[4;33m"
#define UBLU "\033[4;34m"
#define UMAG "\033[4;35m"
#define UCYN "\033[4;36m"
#define UWHT "\033[4;37m"

//Regular background
#define BLKB "\033[40m"
#define REDB "\033[41m"
#define GRNB "\033[42m"
#define YELB "\033[43m"
#define BLUB "\033[44m"
#define MAGB "\033[45m"
#define CYNB "\033[46m"
#define WHTB "\033[47m"

//High intensty background 
#define BLKHB "\033[0;100m"
#define REDHB "\033[0;101m"
#define GRNHB "\033[0;102m"
#define YELHB "\033[0;103m"
#define BLUHB "\033[0;104m"
#define MAGHB "\033[0;105m"
#define CYNHB "\033[0;106m"
#define WHTHB "\033[0;107m"

//High intensty text
#define HBLK "\033[0;90m"
#define HRED "\033[0;91m"
#define HGRN "\033[0;92m"
#define HYEL "\033[0;93m"
#define HBLU "\033[0;94m"
#define HMAG "\033[0;95m"
#define HCYN "\033[0;96m"
#define HWHT "\033[0;97m"

//Bold high intensity text
#define BHBLK "\033[1;90m"
#define BHRED "\033[1;91m"
#define BHGRN "\033[1;92m"
#define BHYEL "\033[1;93m"
#define BHBLU "\033[1;94m"
#define BHMAG "\033[1;95m"
#define BHCYN "\033[1;96m"
#define BHWHT "\033[1;97m"

//Reset
#define COLOR_RESET "\033[0m"

typedef struct memBlock {
    char *memory;
    size_t size;
} memBlock;

void resetSTDColors();
void printInfo(const char *caller, bool extraInfo, char *why, ...);
void printError(const char *caller, bool extraInfo, const char *colorStr, char *why, ...);
void vprintError(const char *caller, const char *colorStr, char *why, va_list arg);
void fatalError_abort(const char *caller, char *why, ...);
void fatalError_exit(const char *caller, char *why, ...);
int msleep(unsigned long msec);
size_t curlMemCb (void *contents, size_t size, size_t quantity, void *obj);
memBlock *initBlock();
void freeBlock(memBlock *mem);
size_t curlMemCb (void *contents, size_t size, size_t quantity, void *obj); 
CURLcode getRequest(progConfig *conf, const char *url, void *data, size_t (*func)(void *contents, size_t size, size_t quantity, void *obj));
size_t dlFileCb(void *ptr, size_t size, size_t nmemb, void *stream);
CURLcode dlFile(progConfig *conf, const char *url, const char *fileName);
char **matchReg(const char *str, const char *regStr, int maxMatches);
int parseStrToInt(const char *str);
double parseStrToDouble(const char *str);
void intToStr(char *buff, int num);
void freeStrArr(char **str);
char *removeExtension(const char *str);
char *getExtension(const char *str);
char *replaceAll(char *str, const char *oldStr, const char *newStr);
void tryFree(void *ptr);
char *fileToMem(const char *filePath);
int freeFileMem(const char *filePath, char *fileStr);
size_t intSize(size_t num);
int isSymlink(const char *path);
char *getRelativePath(const char *fromPath, const char *toPath);
int checkFolder(const char *filePath, bool runMKDIR);
char *genImage(char *cmd, char *filePath, char *imgLabel);
void curlLock(CURL *handle, curl_lock_data data, curl_lock_access laccess, void *userptr);
void curlUnlock(CURL *handle, curl_lock_data data, void *userptr);
char *randStr(size_t size);
char *getSubs(progConfig *conf, char *fileStr, char *filePath);
int getTmdbID(char *URLStr, progConfig *conf);
char *getPoster(const char *posterURL, progConfig *conf, int prefImgWidth, double prefImgRatio, char *prefImgLang);
char *compressImg(const char *convCmd, char *fileToConvert, bool overwrite);
void printBits(unsigned bits);
void printBitFlags(unsigned bits);
int fixMode(progConfig *conf, progFlags flags, const char *toFix, const char *id, const char *poster, const char *newName, bool refreshMode);
int writeCharToFile(const char *str, const char *fileStr);
long double getElapsedTime();
char *appendSlash(char *origStr);
