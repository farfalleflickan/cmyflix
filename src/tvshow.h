#pragma once
#include "conf.h"
#include "fileList.h"

#define TV_HTML_TOP "<!DOCTYPE html>\n<html>\n<head>\n<title>cmyflix</title>\n<meta http-equiv=\"Content-type\" content=\"text/html;charset=UTF-8\">\n<meta name=\"description\" content=\"Daria Rostirolla\">\n<meta name=\"keywords\" content=\"HTML, CSS\">\n<meta name=\"author\" content=\"Daria Rostirolla\">\n<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">\n<link href=\"css/tv.css\" rel=\"stylesheet\" type=\"text/css\">\n<link rel=\"icon\" type=\"image/png\" href=\"img/favicon.png\">\n</head>\n<body>\n<script type=\"text/javascript\" src=\"js/mainTVScript.js\"></script>\n<div id=\"wrapper\">\n"

#define TV_HTML_FRAME "<div class=\"showDiv\">\n<input id=\"A%d\" class=\"myBtn\" alt=\"%s\" onclick=\"javascript:setFrame(this, \'%s\' )\" type=\"image\" src=\"%s\" onload=\"javascript:setAlt(this, \'%s\')\">\n<div id=\"B%d\" class=\"modal\">\n<div id=\"frameDiv%d\" class=\"modal-content\">\n<iframe id=\"IN%d\" src=\"\" frameborder=\"0\" onload=\"javascript:resizeFrame(this)\" allowfullscreen></iframe>\n</div>\n</div>\n</div>\n"

#define TV_HTML_BOT "\n<div id=\"paddingDiv\">\n</div>\n</div>\n</body>\n</html>"

#define SHOW_HTML_TOP "<!DOCTYPE html><html><head><title>cmyflix</title><meta http-equiv=\"Content-type\" content=\"text/html;charset=UTF-8\"><meta name=\"description\" content=\"Daria Rostirolla\"><meta name=\"keywords\" content=\"HTML, CSS\"><meta name=\"author\" content=\"Daria Rostirolla\"><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\"><link href=\"../css/tv.css\" rel=\"stylesheet\" type=\"text/css\"><link rel=\"icon\" type=\"image/png\" href=\"../img/favicon.png\"></head><body>\n<span onclick=\"javascript:hideModal()\" class=\"close\">&times;</span>\n"

#define SHOW_HTML_CMD "\n</video>\n<div class=\"nextEpDiv\">\n<input class=\"nextEpButton\" onclick=\"javascript:resetPlayer()\" type=\"button\" value=\"Reset Player\">\n<input class=\"prevEpButton\" onclick=\"javascript:prevEp()\" type=\"button\" value=\"Prev episode\" >\n<input class=\"nextEpButton\" onclick=\"javascript:nextEp()\" type=\"button\" value=\"Next episode\">\n<label class=\"autoButtonLabel\">\n<input class=\"autoButton\" onclick=\"javascript:autoSwitch()\" type=\"checkbox\" value=\"Automatic\">Automatic</label>\n</div>\n</div>\n</div>\n</li>"

#define SHOW_HTML_SEL "<select id=\"selector%d_\" onchange=\"javascript:changeSeason(this)\" class=\"showSelect\">"

#define SHOW_HTML_OPT_SEASON "\n<option value=\"%d\">Season %d</option>"

#define SHOW_HTML_OPT_EXTRAS "\n<option value=\"%d\">Extras</option>" 

#define SHOW_HTML_UL "\n<ul id=\"C%d_%d\" class=\"showEpUl\">"

#define SHOW_HTML_LI "\n<li>\n<input id=\"D%d_%d\" class=\"epButton\" onclick=\"javascript:showVideoModal(this)\" type=\"button\" value=\"%s\">\n"

#define SHOW_HTML_DIV "<div id=\"E%d_%d\" class=\"modal\">\n<div class=\"modal-content\">"

#define SHOW_HTML_SPAN "<span onclick=\"javascript:hideVideoModal()\" class=\"close\">&times;</span>\n<p id=\"epTitle\">%s</p>"

#define SHOW_HTML_VIDEO "\n<video id=\"F%d_%d\" class=\"video_player\" controls preload=\"none\" onplaying=\"javascript:rezHandler()\">\n<source src=\"../%s\" type=\"video/mp4\">"

#define SHOW_HTML_SUBS "\n<track src=\"../%s\" kind=\"subtitles\" label=\"%s\">"

#define SHOW_HTML_BOT "\n<script type=\"text/javascript\" src=\"../js/TVScript.js\" onload=\"javascript:showModal('selector%d_')\"></script></body></html>"

struct fileList *createTVShowDB(progConfig *conf);
void *findSeasons(void *threadArg);
void *findEpisodes(void *threadArg);

void createShowsHTML(progConfig *conf, fileList *list);
void *showHTML(void *threadArg);
void *cleanTV(void *threadArg);
void *cleanShow(void *threadArg);
void episodeHTML(fileList *this_show, progConfig *conf, cJSON *episode, int *currSeason, int *uuid, int *uuidEpisode);
char *getShowPoster(progConfig *conf, unsigned int tmdb_id);
char *getEpisodeName(progConfig *conf, unsigned int showId, char *seNum, char *epNum, char *TMDBapi);
int getShowID(progConfig *conf, char *tvShowName);
