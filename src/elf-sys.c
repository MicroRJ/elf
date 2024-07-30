/*
** See Copyright Notice In elf.h
** elf-sys.c
** System
*/


#if defined(PLATFORM_WEB)
#include <sys/types.h>
#include <dlfcn.h>
#include <dirent.h>
#elif defined(PLATFORM_DESKTOP)
#pragma comment(lib,"user32")
#pragma comment(lib,"Ws2_32")
#define WIN32_LEAN_AND_MEAN
#if !defined(ELF_KEEPWINDOWS)
/* todo: should probaly just define the functions
I want to use instead! */
#define NOGDICAPMASKS //     - CC_*, LC_*, PC_*, CP_*, TC_*, RC_
#define NOVIRTUALKEYCODES // - VK_*
#define NOWINMESSAGES //     - WM_*, EM_*, LB_*, CB_*
#define NOWINSTYLES //       - WS_*, CS_*, ES_*, LBS_*, SBS_*, CBS_*
#define NOSYSMETRICS //      - SM_*
#define NOMENUS //           - MF_*
#define NOICONS //           - IDI_*
#define NOKEYSTATES //       - MK_*
#define NOSYSCOMMANDS //     - SC_*
#define NORASTEROPS //       - Binary and Tertiary raster ops
#define NOSHOWWINDOW //      - SW_*
#define OEMRESOURCE //       - OEM Resource values
#define NOATOM //            - Atom Manager routines
#define NOCLIPBOARD //       - Clipboard routines
#define NOCOLOR //           - Screen colors
#define NOCTLMGR //          - Control and Dialog routines
#define NODRAWTEXT //        - DrawText() and DT_*
#define NOGDI //             - All GDI defines and routines
#define NOKERNEL //          - All KERNEL defines and routines
#define NOUSER //            - All USER defines and routines
#define NONLS //             - All NLS defines and routines
#define NOMB //              - MB_* and MessageBox()
#define NOMEMMGR //          - GMEM_*, LMEM_*, GHND, LHND, associated routines
#define NOMETAFILE //        - typedef METAFILEPICT
#define NOMINMAX //          - Macros min(a,b) and max(a,b)
#define NOMSG //             - typedef MSG and associated routines
#define NOOPENFILE //        - OpenFile(), OemToAnsi, AnsiToOem, and OF_*
#define NOSCROLL //          - SB_* and scrolling routines
#define NOSERVICE //         - All Service Controller routines, SERVICE_ equates, etc.
#define NOSOUND //           - Sound driver routines
#define NOTEXTMETRIC //      - typedef TEXTMETRIC and associated routines
#define NOWH //              - SetWindowsHook and WH_*
#define NOWINOFFSETS //      - GWL_*, GCL_*, associated routines
#define NOCOMM //            - COMM driver routines
#define NOKANJI //           - Kanji support stuff.
#define NOHELP //            - Help engine interface.
#define NOPROFILER //        - Profiler interface.
#define NODEFERWINDOWPOS //  - DeferWindowPos routines
#define NOMCX //             - Modem Configuration Extensions
#endif
#include <windows.h>
#include <Windowsx.h>
#include <Winsock2.h>
#include <ws2tcpip.h>
#include   <ws2def.h>
#include <shellapi.h>
#else
#endif



elAPI elBool sys_debugger() {
#if defined(PLATFORM_DESKTOP)
	// fclose(_logging_io);
	DebugBreak();
	return 1;
#elif defined(PLATFORM_WEB)
	emscripten_debugger();
	return 1;
#else
	return 0;
#endif
}


elAPI void sys_consolelog(int type, char *message) {
#if defined(PLATFORM_DESKTOP)
	/* bruh */
	elf_log(type,"%s",message);
#else
	switch (type) {
		case ELF_LOGDBUG: case ELF_LOGINFO: {
			type = EM_LOG_CONSOLE;
		} break;
		case ELF_LOGERROR: case ELF_LOGFATAL: {
			type = EM_LOG_ERROR;
		} break;
		case ELF_LOGWARN: {
		 	type = EM_LOG_WARN;
		} break;
	}
	emscripten_log(type,message);
#endif
}


elAPI int sys_getlasterror() {
#if defined(PLATFORM_DESKTOP)
	return GetLastError();
#else
	return 0;
#endif
}


elAPI void sys_geterrormsg(int error, char *buf, int len) {
#if defined(PLATFORM_DESKTOP)
	if (error == 0) error = GetLastError();
	FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM,0x00,error,LANG_USER_DEFAULT,buf,len,NULL);
#endif
}


elAPI void *sys_valloc(elInteger length) {
#if defined(PLATFORM_DESKTOP)
	return VirtualAlloc(NULL,length,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);
#else
	return elNil;
#endif
}


elAPI void sys_sleep(elInteger ms) {
#if defined(PLATFORMPLATFORM_WIN32)
	Sleep((DWORD) ms);
#elif defined(PLATFORM_WEB)
	emscripten_sleep(ms);
#endif
}


elAPI elInteger sys_clockhz() {
#if defined(PLATFORM_WEB)
	return 1000;
#elif defined(PLATFORM_DESKTOP)
	LARGE_INTEGER largeInt;
	QueryPerformanceFrequency(&largeInt);
	return largeInt.QuadPart;
#else
	return 0;
#endif
}


elAPI elInteger sys_clocktime() {
#if defined(PLATFORM_DESKTOP)
	LARGE_INTEGER largeInt;
	QueryPerformanceCounter(&largeInt);
	return largeInt.QuadPart;
#elif defined(PLATFORM_WEB)
	return emscripten_get_now();
#else
	return 0;
#endif
}


elAPI int sys_getmyname(int length, char *buffer) {
#if defined(PLATFORM_DESKTOP)
	return GetModuleFileName(NULL,buffer,length);
#else
	return 0;
#endif
}


elAPI int sys_getmypid() {
#if defined(PLATFORM_DESKTOP)
	return GetCurrentProcessId();
#else
	return 0;
#endif
}


elAPI int sys_getworkdir(int length, char *buffer) {
#if defined(PLATFORM_DESKTOP)
	return GetCurrentDirectory(length,buffer);
#else
	return 0;
#endif
}


elAPI int sys_changeworkdir(char *buffer) {
#if defined(PLATFORM_DESKTOP) && defined(_WIN32)
	return SetCurrentDirectory(buffer);
#else
	return chdir(buffer);
#endif
}


elAPI elHandle sys_loadlib(char const *name) {
#if defined(PLATFORM_DESKTOP)
	return (elHandle) LoadLibraryA(name);
#elif defined(PLATFORM_WEB)
	#if 0
	em_promise_t promise = emscripten_dlopen_promise(name,RTLD_LAZY);
	em_settled_result_t result = emscripten_promise_await(promise);
	emscripten_promise_destroy(promise);
	return (elHandle) result.value;
	#endif
	return 0;
#else
	void *handle = dlopen(name,RTLD_LAZY);
	if (handle == elNil) {
		sys_consolelog(ELF_LOGERROR,"the following is a system error:");
		sys_consolelog(ELF_LOGERROR,dlerror());
		sys_consolelog(ELF_LOGERROR,"end");
	}
	return (elHandle) handle;
#endif
}


elAPI void *sys_libfn(elHandle dll, char const *name) {
#if defined(PLATFORM_DESKTOP)
	return (void *) GetProcAddress(dll,name);
#else
	return (void *) dlsym(dll,name);
#endif
}


elAPI elError sys_load_file_contents(Alloc *allocfn, void **data, char const *name) {

	elError error = Error_None;

	if (name == elNil) {
		error = Error_FileNameIsInvalid;
		goto leave;
	}
	if (data == elNil) {
		error = Error_InvalidArguments;
		goto leave;
	}

	*data = elNil;
#if defined(PLATFORM_DESKTOP)
	HANDLE hfile = CreateFileA(name,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,0x00,NULL);
	if (hfile != INVALID_HANDLE_VALUE) {
		DWORD hi,lo = GetFileSize(hfile,&hi);
		char *buf = elf_alloc(allocfn,lo+1);
		DWORD bytes;
		if (ReadFile(hfile,buf,lo,&bytes,NULL)) {
			buf[bytes] = 0;
			*data = buf;
			if (bytes != lo) {
				error = Error_CouldNotReadEntireFile;
				goto leave;
			}
		} else {
			elf_dealloc(allocfn,buf);
			error = Error_CouldNotReadFile;
			goto leave;
		}
		CloseHandle(hfile);
	} else {
		DWORD lasterror = GetLastError();
		if (lasterror == ERROR_FILE_NOT_FOUND) {
			error = Error_FileNotFound;
		} else {
			error = Error_CouldNotLoadFile;
		}
	}
#else
	FILE *file = fopen(name,"rb");
	if (file == elNil) {
		error = Error_FileNotFound;
		goto leave;
	}
	fseek(file,0,SEEK_END);
	long fileSize = ftell(file);
	fseek(file,0,SEEK_SET);
	char *buf = (char *) elf_alloc(allocfn,fileSize+1);
	fread(buf,1,fileSize,file);
	fclose(file);
	buf[fileSize] = 0;
	*data = buf;
#endif
	leave:
	// if LPASSED(error) {
	// 	elf_loginfo("'%s': file loaded",name);
	// } else {
	// 	elf_loginfo("'%s': failed to load file, %s",name,ERNAME(error));
	// }
	return error;

}


elAPI elError sys_savefilebytes(char const *buffer, elInteger length, char const *fileName) {
	FILE *file;
#if defined(_MSC_VER)
	fopen_s(&file,fileName,"wb");
#else
	file = fopen(fileName,"wb");
#endif

	if (file == elNil) {
		return Error_CouldNotOpenFile;
	}

	elError error = Error_None;
	elInteger lengthWritten = fwrite(buffer, 1, length, file);

	if (lengthWritten != length) {
		error = Error_CouldNotWriteEntireFile;
	}

	fclose(file);

	return error;
}

