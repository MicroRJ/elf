.PHONY: all clean
# to use make for windows, get the toolkit from:
# git @ skeeto/w64devkit/releases
# this makefile needs working...
# if you're using clang on windows, ensure you're
# either using the developer console or you've
# setup the path properly using vcvarsall or
# vcvars64 or vcvars32
MAKE ?= make
CC = clang
PLATFORM ?= PLATFORM_DESKTOP
OUT = build/elf.exe
CFLAGS = -D$(PLATFORM) -Wall
ifeq ($(MODE),RELEASE)
	CFLAGS += -O3
else
	CFLAGS += -g
	CFLAGS += -O0
	CFLAGS += -D_DEBUG
endif
ifeq ($(PLATFORM),PLATFORM_WEB)
	CC = emcc
	OUT = build/elf.html
endif
all: build/elf.exe
build/elf.exe: elf.h elf.c $(wildcard src/*)
	$(CC) $(CFLAGS) elf.c -o $(OUT) -I.
clean:
	rm -f build/*
	rm -f build/web/*