//
// See Copyright Notice In elf.h
//

#pragma comment(lib, "user32")
#pragma comment(lib, "Ws2_32")

#define WIN32_LEAN_AND_MEAN
#define NOGDICAPMASKS
#define NOVIRTUALKEYCODES
#define NOWINMESSAGES
#define NOWINSTYLES
#define NOSYSMETRICS
#define NOMENUS
#define NOICONS
#define NOKEYSTATES
#define NOSYSCOMMANDS
#define NORASTEROPS
#define NOSHOWWINDOW
#define OEMRESOURCE
#define NOATOM
#define NOCLIPBOARD
#define NOCOLOR
#define NOCTLMGR
#define NODRAWTEXT
#define NOGDI
#define NOKERNEL
#define NOUSER
#define NONLS
#define NOMB
#define NOMEMMGR
#define NOMETAFILE
#define NOMINMAX
#define NOMSG
#define NOOPENFILE
#define NOSCROLL
#define NOSERVICE
#define NOSOUND
#define NOTEXTMETRIC
#define NOWH
#define NOWINOFFSETS
#define NOCOMM
#define NOKANJI
#define NOHELP
#define NOPROFILER
#define NODEFERWINDOWPOS
#define NOMCX

#include <windows.h>
#include <Windowsx.h>
#include <shellapi.h>

#include "elf.h"
#include "base.h"
#include "platform.h"

STATIC_ASSERT(sizeof(elf_PlatformFile) >= sizeof(HANDLE));

static HANDLE win32_handle(elf_PlatformFile file)
{
	return (HANDLE)(uintptr_t)file;
}

static elf_PlatformFile elf_platform_file_from_win32(HANDLE handle)
{
	return (elf_PlatformFile)(uintptr_t)handle;
}

#include "platform_console_win32.c"
#include "platform_file_win32.c"
#include "platform_memory_win32.c"
#include "platform_time_win32.c"
#include "platform_process_win32.c"
#include "platform_dll_win32.c"
