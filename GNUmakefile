include vars.mk

EXE     =me7sum$(EXE_EXT)
LIBS    =ini
SUBDIRS =inifile

ifneq (, $(findstring mingw, $(SYS)))
LIBS += ws2_32
SRC += os/pgetopt.c
endif

include makefile.common

.PHONY: test
test:
	./test.sh

win: force
	./build.cmd clean
	./build.cmd

INIS=sample.ini # bins/ferrari360.ini bins/8D0907551M.ini
.PHONY: zip
zip: win
	zip -j me7sum-$(GIT_VERSION).zip me7sum.exe ME7Check.exe README.md $(INIS)
