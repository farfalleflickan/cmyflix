FROM alpine
RUN apk add git make bash gcc build-base libbsd-dev cjson-dev curl-dev nginx rsvg-convert
RUN git clone https://github.com/farfalleflickan/cmyflix app
RUN make -C app
RUN mkdir Movies TV
CMD app/bin/cmyflix
RUN echo "server { \
    listen 80; \
    server_name cmyflix; \ 
    root /etc/cmyflix/output/; \
    location /TV/ { \
        try_files $uri $uri/ /TV.html; \
    } \
    location /movies/ { \
        try_files $uri $uri/ /Movies.html; \
    } \
}" >> /etc/nginx/http.d/cmyflix.conf
CMD ["nginx", "-g", "daemon off;"]
