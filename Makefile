.PHONY: all clean
# to use make for windows, get the toolkit from:
# git @ skeeto/w64devkit/releases
# this makefile needs working...
MAKE ?= make
CC = clang-cl
PLATFORM ?= PLATFORM_DESKTOP
CFLAGS = -D$(PLATFORM)
OUT = build
ifeq ($(PLATFORM),PLATFORM_WEB)
	CC = emcc
	OUT = build/web
	CFLAGS += -O3 -Wall
else ifeq ($(PLATFORM),PLATFORM_DESKTOP)
	CFLAGS += -TC -Z7 -W4
	CFLAGS += -Od -MTd
	CFLAGS += -D_DEBUG
endif
all: build/elf.exe
build/elf.exe: elf.h elf.c $(wildcard src/*)
	$(CC) $(CFLAGS) elf.c -o build/elf.exe -I.
clean:
	del build/* /s