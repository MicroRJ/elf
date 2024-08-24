/*
** See Copyright Notice In elf.h
** system.c
** Basic system layer
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



elBool sys_debugger() {
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


void sys_console_print(int type, char *message) {
#if defined(PLATFORM_DESKTOP)
	/* bruh */
	elf_log(type,"%s",message);
#else
	switch (type) {
		case LOG_KDEBUG: case LOG_KINFO: {
			type = EM_LOG_CONSOLE;
		} break;
		case LOG_KERROR: case LOG_KFATAL: {
			type = EM_LOG_ERROR;
		} break;
		case LOG_KWARNING: {
		 	type = EM_LOG_WARN;
		} break;
	}
	emscripten_log(type,message);
#endif
}


int sys_get_last_error() {
#if defined(PLATFORM_DESKTOP)
	return GetLastError();
#else
	return 0;
#endif
}


void sys_get_error_msg(int error, char *buf, int len) {
#if defined(PLATFORM_DESKTOP)
	if (error == 0) error = GetLastError();
	FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM,0x00,error,LANG_USER_DEFAULT,buf,len,NULL);
#endif
}


void *sys_valloc(elInteger length) {
#if defined(PLATFORM_DESKTOP)
	return VirtualAlloc(NULL,length,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);
#else
	return 0;
#endif
}


void sys_sleep(elInteger ms) {
#if defined(PLATFORMPLATFORM_WIN32)
	Sleep((DWORD) ms);
#elif defined(PLATFORM_WEB)
	emscripten_sleep(ms);
#endif
}


elInteger sys_get_clock_freq() {
#if defined(PLATFORM_WEB)
	return 1000;
#elif defined(PLATFORM_DESKTOP)
	LARGE_INTEGER large_integer;
	QueryPerformanceFrequency(&large_integer);
	return large_integer.QuadPart;
#else
	return 0;
#endif
}


elInteger sys_get_clock_time() {
#if defined(PLATFORM_DESKTOP)
	LARGE_INTEGER large_integer;
	QueryPerformanceCounter(&large_integer);
	return large_integer.QuadPart;
#elif defined(PLATFORM_WEB)
	return emscripten_get_now();
#else
	return 0;
#endif
}


int sys_get_my_name(int length, char *buffer) {
#if defined(PLATFORM_DESKTOP)
	return GetModuleFileName(NULL,buffer,length);
#else
	return 0;
#endif
}


int sys_get_my_pid() {
#if defined(PLATFORM_DESKTOP)
	return GetCurrentProcessId();
#else
	return 0;
#endif
}


int sys_get_work_dir(int length, char *buffer) {
#if defined(PLATFORM_DESKTOP)
	return GetCurrentDirectory(length,buffer);
#else
	return 0;
#endif
}


int sys_set_work_dir(char *buffer) {
#if defined(PLATFORM_DESKTOP) && defined(_WIN32)
	return SetCurrentDirectory(buffer);
#else
	return !chdir(buffer);
#endif
}


elHandle sys_load_dll(char const *name) {
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
	if (handle == 0) {
		sys_console_print(LOG_KERROR,"the following is a system error:");
		sys_console_print(LOG_KERROR,dlerror());
		sys_console_print(LOG_KERROR,"end");
	}
	return (elHandle) handle;
#endif
}


void *sys_get_dll_fn(elHandle dll, char const *name) {
#if defined(PLATFORM_DESKTOP)
	return (void *) GetProcAddress(dll,name);
#else
	return (void *) dlsym(dll,name);
#endif
}


elError sys_load_file_data(elAllocator fn, void **data, char const *name) {

	elError error = Error_None;

	if (name == 0) {
		error = Error_FileNameIsInvalid;
		goto esc;
	}
	if (data == 0) {
		error = Error_InvalidArguments;
		goto esc;
	}

	*data = 0;
#if defined(PLATFORM_DESKTOP)
	HANDLE hfile = CreateFileA(name,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,0x00,NULL);
	if (hfile != INVALID_HANDLE_VALUE) {
		DWORD hi,lo = GetFileSize(hfile,&hi);
		char *buf = elf_alloc(fn,lo+1);
		DWORD bytes;
		if (ReadFile(hfile,buf,lo,&bytes,NULL)) {
			buf[bytes] = 0;
			*data = buf;
			if (bytes != lo) {
				error = Error_CouldNotReadEntireFile;
				goto esc;
			}
		} else {
			elf_dealloc(fn,buf);
			error = Error_CouldNotReadFile;
			goto esc;
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
	if (file == 0) {
		error = Error_FileNotFound;
		goto esc;
	}
	fseek(file,0,SEEK_END);
	long fileSize = ftell(file);
	fseek(file,0,SEEK_SET);
	char *buf = (char *) elf_alloc(fn,fileSize+1);
	fread(buf,1,fileSize,file);
	fclose(file);
	buf[fileSize] = 0;
	*data = buf;
#endif
	esc:
	// if PASSED(error) {
	// 	elf_info_log("'%s': file loaded",name);
	// } else {
	// 	elf_info_log("'%s': failed to load file, %s",name,ERNAME(error));
	// }
	return error;

}


elError sys_save_file_data(char const *buffer, elInteger length, char const *fileName) {
	FILE *file;
#if defined(_MSC_VER)
	fopen_s(&file,fileName,"wb");
#else
	file = fopen(fileName,"wb");
#endif

	if (file == 0) {
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


int sys_shell(char const *verb, char const *file, char const *args) {
#if defined(PLATFORM_DESKTOP)
	return (INT_PTR)ShellExecute(NULL,verb,file,args,NULL,10) > 32;
#endif
	return 0;
}


int sys_exec(char const *file, char const *args) {
#if defined(PLATFORM_DESKTOP)
	STARTUPINFO si = {sizeof(si)};
	PROCESS_INFORMATION pi = {0};
	int result = CreateProcess(file,(char*)args,NULL,NULL,FALSE,0,NULL,NULL,&si,&pi);
	CloseHandle(pi.hProcess);
	CloseHandle(pi.hThread);
	return result;
#endif
	return -1;
}
