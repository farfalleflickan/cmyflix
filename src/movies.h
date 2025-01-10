#pragma once
#include "conf.h"
#include "fileList.h"

#define MOVIE_HTML_TOP "<!DOCTYPE html>\n<html>\n<head>\n<title>cmyflix</title>\n<meta http-equiv=\"Content-type\" content=\"text/html;charset=UTF-8\">\n<meta name=\"description\" content=\"Daria Rostirolla\">\n<meta name=\"keywords\" content=\"HTML, CSS\">\n<meta name=\"author\" content=\"Daria Rostirolla\">\n<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">\n<link href=\"css/movie.css\" rel=\"stylesheet\" type=\"text/css\">\n<link rel=\"icon\" type=\"image/png\" href=\"img/favicon.png\">\n</head>\n<body>\n<script type=\"text/javascript\" src=\"js/Mscript.js\"></script><div id=\"wrapper\">"

#define MOVIE_HTML_VIDEO        "<div class=\"movieDiv\">\n<input id=\"A%d\" class=\"myBtn\" alt=\"%s\" onclick=\"javascript:showModal(this)\" type=\"image\" src=\"%s\" onload=\"javascript:setAlt(this, '%s')\">\n<div id=\"B%d\" class=\"modal\">\n<div class=\"modal-content\">\n<video id=\"C%d\" class=\"video_player\" controls preload=\"none\">\n<source src=\"%s\">"
#define MOVIE_HTML_VIDEO_WEBM   "<div class=\"movieDiv\">\n<input id=\"A%d\" class=\"myBtn\" alt=\"%s\" onclick=\"javascript:showModal(this)\" type=\"image\" src=\"%s\" onload=\"javascript:setAlt(this, '%s')\">\n<div id=\"B%d\" class=\"modal\">\n<div class=\"modal-content\">\n<video id=\"C%d\" class=\"video_player\" controls preload=\"none\">\n<source src=\"%s\" type=\"video/webm\">"
#define MOVIE_HTML_VIDEO_MP4    "<div class=\"movieDiv\">\n<input id=\"A%d\" class=\"myBtn\" alt=\"%s\" onclick=\"javascript:showModal(this)\" type=\"image\" src=\"%s\" onload=\"javascript:setAlt(this, '%s')\">\n<div id=\"B%d\" class=\"modal\">\n<div class=\"modal-content\">\n<video id=\"C%d\" class=\"video_player\" controls preload=\"none\">\n<source src=\"%s\" type=\"video/mp4\">"
#define MOVIE_HTML_VIDEO_OGG    "<div class=\"movieDiv\">\n<input id=\"A%d\" class=\"myBtn\" alt=\"%s\" onclick=\"javascript:showModal(this)\" type=\"image\" src=\"%s\" onload=\"javascript:setAlt(this, '%s')\">\n<div id=\"B%d\" class=\"modal\">\n<div class=\"modal-content\">\n<video id=\"C%d\" class=\"video_player\" controls preload=\"none\">\n<source src=\"%s\" type=\"video/ogg\">"

#define MOVIE_HTML_SUBS "\n<track src=\"%s\" kind=\"subtitles\" label=\"%s\">"

#define MOVIE_HTML_VIDEO_BOTTOM "\n</video>\n<span onclick=\"javascript:hideModal()\" class=\"close\">&times;</span>\n</div>\n</div>\n</div>"

#define MOVIE_HTML_BOT "\n<div id=\"paddingDiv\">\n</div>\n</div>\n</body>\n</html>"

struct fileList *createMoviesDB(progConfig *conf);
void *findMovies(void *threadArg);
void *movieHTML(void *threadArg);
void *cleanMovies(void *threadArg);
void createMoviesHTML(progConfig *conf, fileList *list);
int getMovieID(progConfig *conf, char *movieName);
char *getMoviePoster(progConfig *conf, int tmdb_id);
