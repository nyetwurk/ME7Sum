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
	scripts/test.sh

# corpus: fetch the private ecu-corpus submodule at its pinned commit (needs access)
# corpus-bump: move it to the ecu-corpus head (commit it yourself)
.PHONY: corpus corpus-bump
corpus:
	git submodule update --init --checkout --depth 1 corpus

corpus-bump:
	git submodule update --init --checkout --remote --depth 1 corpus
	@git -C corpus log -1 --format='corpus now at %h %s'
	@git status --short corpus

# bins: temporary, slated for removal; symlinks the old bins/ names into corpus/ and testdata/
.PHONY: bins
bins:
	scripts/bins.sh

win: force
	./build.cmd clean
	./build.cmd

INIS=sample.ini # testdata/broken/ferrari360.ini testdata/modified/8D0907551M.ini
.PHONY: zip
zip: win
	zip -j me7sum-$(GIT_VERSION).zip me7sum.exe ME7Check.exe README.md $(INIS)
