/*
** See Copyright Notice In elf.h
** system.c
*/

#if defined(PLATFORM_WEB)
#include <sys/types.h>
#include <dlfcn.h>
#include <dirent.h>
#elif defined(PLATFORM_DESKTOP)
#pragma comment(lib,"user32")
#pragma comment(lib,"Ws2_32")
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <Windowsx.h>
#include <Winsock2.h>
#include <ws2tcpip.h>
#include   <ws2def.h>
#include <shellapi.h>
#else
#endif



elf_Bool sys_debugger() {
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

void *sys_virtual_alloc(elf_i64 length) {
#if defined(PLATFORM_DESKTOP)
	return VirtualAlloc(NULL,length,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);
#else
	return 0;
#endif
}

void sys_virtual_dealloc(void *memory) {
#if defined(PLATFORM_DESKTOP)
	VirtualFree(memory,0,MEM_RELEASE);
#else
	return 0;
#endif
}


void sys_sleep(elf_Int ms) {
#if defined(PLATFORMPLATFORM_WIN32)
	Sleep((DWORD) ms);
#elif defined(PLATFORM_WEB)
	emscripten_sleep(ms);
#endif
}


elf_Int sys_get_clock_freq() {
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


elf_Int sys_get_clock_time() {
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


elf_Handle sys_load_dll(char const *name) {
#if defined(PLATFORM_DESKTOP)
	return (elf_Handle) LoadLibraryA(name);
#elif defined(PLATFORM_WEB)
	#if 0
	em_promise_t promise = emscripten_dlopen_promise(name,RTLD_LAZY);
	em_settled_result_t result = emscripten_promise_await(promise);
	emscripten_promise_destroy(promise);
	return (elf_Handle) result.value;
	#endif
	return 0;
#else
	void *handle = dlopen(name,RTLD_LAZY);
	if (handle == 0) {
		sys_console_print(LOG_KERROR,"the following is a system error:");
		sys_console_print(LOG_KERROR,dlerror());
		sys_console_print(LOG_KERROR,"end");
	}
	return (elf_Handle) handle;
#endif
}


void *sys_get_dll_fn(elf_Handle dll, char const *name) {
#if defined(PLATFORM_DESKTOP)
	return (void *) GetProcAddress(dll,name);
#else
	return (void *) dlsym(dll,name);
#endif
}


// int sys_exec(char const *file, char const *args) {
// #if defined(PLATFORM_DESKTOP)
// 	STARTUPINFO si = {sizeof(si)};
// 	PROCESS_INFORMATION pi = {0};
// 	int result = CreateProcess(file,(char*)args,NULL,NULL,FALSE,0,NULL,NULL,&si,&pi);
// 	CloseHandle(pi.hProcess);
// 	CloseHandle(pi.hThread);
// 	return result;
// #endif
// 	return -1;
// }



// todo: might actually use this instead of window's, and then
// i can just put it in lib core directly.
#if 0
	// defined(PLATFORM_WEB)
	DIR *dirfd = opendir(dir->c);
	if (dirfd != 0) {
		struct dirent *entry;
		while ((entry = readdir(dirfd)) != 0) {
			if (elf_is_virtual_file_name(entry->d_name)) {
				continue;
			}
			elf_Bool isdir = (entry->d_type & DT_DIR) != 0;
			elf_Value *top = GET_TOP(R);

			elf_String *name = elf_new_string(R,entry->d_name);
			elf_String *path = elf_new_string(R,elf_tpf("%s/%s",dir->c,entry->d_name));
			elf_StackId base = elf_add_closure(R,cls);
			elf_Table *file = elf_new_table(R);

			elf_tsets_str(file,elf_new_string(R,"name"),name);
			elf_tsets_str(file,elf_new_string(R,"path"),path);
			elf_tsets_int(file,elf_new_string(R,"isdir"),isdir);
			int r = elf_call(R,base,1,1);
			if ((r > 0) && isdir && elf_get_int(R,base)) {
				core_lib_enumerate_folder_(R,path,cls);
			}
			SET_TOP(R,top);
		}
		closedir(dirfd);

#endif