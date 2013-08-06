RM	= rm -f
AR	= ar rcs
ECHO	= @echo
CFLAGS	= -Wall -O3 -Werror -MD $(CDEFS)

CDEFS	+= -D__GIT_VERSION=\"$(GIT_VERSION)\"

GIT_VERSION = $(shell sh -c 'git describe --tags --abbrev=4 --dirty --always')
SYS	:= $(shell gcc -dumpmachine)

ifneq (, $(findstring mingw, $(SYS)))
EXE_EXT = .exe
CC	= i686-pc-mingw32-gcc 
else ifneq (, $(findstring cygwin, $(SYS)))
EXE_EXT = .exe
CC	= i686-pc-cygwin-gcc 
else
CC	= gcc
endif

SRC     = $(notdir $(foreach dir, ., $(wildcard $(dir)/*.c)))
