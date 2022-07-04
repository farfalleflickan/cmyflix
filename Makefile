SHELL:=/bin/bash 
CFLAGS=-O2 -Wall -Wpedantic -Werror=vla -std=gnu99 -lcurl -lpthread -lbsd -lcjson -ggdb

BINARY_FILE=cmyflix

BUILD_DIR=bin/
BINARY_PATH=${BUILD_DIR}${BINARY_FILE}

EXTRAS_DIR=extras/

DESTDIR=/

INSTALL_BIN_DIR=${DESTDIR}usr/local/bin/
INSTALL_ETC_DIR=${DESTDIR}etc/cmyflix/

.PHONY: bin clean ctags deb help install run uninstall valgrind callgrind 

bin: 
	mkdir -p ${BUILD_DIR}
	${CC} src/*.c ${CFLAGS} -o ${BINARY_PATH}
	cp -r ${EXTRAS_DIR}* ${BUILD_DIR}

clean:
	rm -rf ${BUILD_DIR}
	@rm -rf deb

ctags:
	@ctags -R --exclude=.git -f .tags src/ extras/

help:
	@echo -e "The following are some of the valid targets for this Makefile:\n\tbin (the default if no target is provided)\n\tclean\n\tctags\n\thelp\n\tinstall\n\trun\n\tuninstall\n\tvalgrind\n\tcallgrind\n\tdeb\n"

install: bin
ifneq ($(shell id -u), 0)
	@echo "You must be root to perform this action."
else
	install -d ${INSTALL_BIN_DIR}
	install -d ${INSTALL_ETC_DIR}
	install ${BINARY_PATH} ${INSTALL_BIN_DIR}
	cp -r ${EXTRAS_DIR}* ${INSTALL_ETC_DIR}
endif

run: bin
	cd ${BUILD_DIR} && exec ./${BINARY_FILE} && cd ${PWD}

uninstall:
ifneq ($(shell id -u), 0)
	@echo "You must be root to perform this action."
else
	rm ${INSTALL_BIN_DIR}${BINARY_FILE}
	rm -rf ${INSTALL_ETC_DIR}
endif

valgrind: bin
	cd ${BUILD_DIR} && valgrind --max-threads=4096 --log-file=valgrindLog.txt --tool=memcheck --leak-check=full --show-leak-kinds=all --track-origins=yes -s ./${BINARY_FILE} && cat valgrindLog.txt

callgrind: bin
	cd ${BUILD_DIR} && valgrind --tool=callgrind --dump-instr=yes --collect-jumps=yes ./${BINARY_FILE}

deb: bin
	@ARCH_STR=$$(if command -v dpkg &> /dev/null; then dpkg --print-architecture; else arch=$$(uname -m); if [[ $$arch == *"i386"* ]]; then echo "i386"; elif [[ $$arch == *"i686"* ]]; then echo "i386"; elif [[ $$arch == *"x86_64"* ]]; then echo "amd64"; elif [[ $$arch == *"aarch64"* ]]; then echo "arm64"; elif [[ $$arch == *"armv8b"* ]]; then echo "arm64"; elif [[ $$arch == *"armv"*"l" ]]; then echo "armhf"; elif [[ $$arch == *"arm"* ]]; then echo "arm"; fi; fi); \
	VERSION_STR=$$(grep -i '#define VERSION_STRING "' src/main.c | sed -e 's/\#define VERSION_STRING\ //g;s/\"//g'); \
	PKG_DIR=${BINARY_FILE}_$${VERSION_STR}_$${ARCH_STR}; \
	PKG_BIN_DIR=deb/$${PKG_DIR}/usr/local/bin/; \
	PKG_ETC_DIR=deb/$${PKG_DIR}/etc/${BINARY_FILE}/; \
	BIN=${BINARY_PATH}; \
	EXTRAS=${EXTRAS_DIR}*; \
	mkdir -p $${PKG_BIN_DIR}; \
	mkdir -p $${PKG_ETC_DIR}; \
	cp $${BIN} $${PKG_BIN_DIR}; \
	cp -r $${EXTRAS}* $${PKG_ETC_DIR}; \
	mkdir deb/$${PKG_DIR}/DEBIAN; \
	echo -e "Package: cmyflix\nVersion: $${VERSION_STR}\nArchitecture: $${ARCH_STR}\nDepends: ffmpeg,imagemagick,libcjson1\nMaintainer: farfalleflickan <farfalleflickan@gmail.com>\nDescription: A static webpage generator for your movies and tv shows.\n For more info see: https://github.com/farfalleflickan/cmyflix" > deb/$${PKG_DIR}/DEBIAN/control; \
	echo -e "#!/usr/bin/make -f\n%:\n    dh $@\noverride_dh_install:\n    dh_install $${PKG_ETC_DIR} /etc/${BINARY_FILE}\n" > deb/$${PKG_DIR}/DEBIAN/rules; \
	cd deb && dpkg-deb --build --root-owner-group $${PKG_DIR} 
