//
// See Copyright Notice In elf.h
//

#pragma comment(lib,"user32")
#pragma comment(lib,"Ws2_32")
#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <Windowsx.h>
#include <Winsock2.h>
#include <ws2tcpip.h>
#include   <ws2def.h>
#include <shellapi.h>

#define FILE_HANDLE HANDLE
#define FILE_TIME FILETIME
#define SYSTEM_TIME SYSTEMTIME

#include "elf_coretypes.h"
#include "system.h"
#include "subsystem.h"

STATIC_ASSERT(sizeof(_FILE_HANDLE) >= sizeof(FILE_HANDLE));


// todo: implement offsets
unsigned int sys_read_file(FILE_HANDLE file, char *buf, unsigned int pos, unsigned int size) {
	DWORD read = 0;
	ReadFile(file, buf, size, &read, NULL);
	return read;
}

unsigned int sys_write_file(FILE_HANDLE file, char *buf, unsigned int pos, unsigned int size) {
	DWORD wrote = 0;
	WriteFile(file, buf, size, &wrote, NULL);
	return wrote;
}

unsigned int sys_size_file(FILE_HANDLE file) {
	DWORD size = GetFileSize(file, NULL);
	return size;
}

void sys_close_file(FILE_HANDLE file) {
	CloseHandle(file);
}

FILE_HANDLE sys_open_file(char *name, int flags, int mode) {

	int os_flags = 0;
	if (flags & SYS_OPEN_READ) os_flags |= GENERIC_READ;
	if (flags & SYS_OPEN_WRITE) os_flags |= GENERIC_WRITE;
	if (flags & SYS_OPEN_EXECUTE) os_flags |= GENERIC_EXECUTE;

	int os_sharing_flags = 0;
	if (flags & SYS_SHARE_READ) os_sharing_flags |= FILE_SHARE_READ;
	if (flags & SYS_SHARE_WRITE) os_sharing_flags |= FILE_SHARE_WRITE;

	int os_mode = OPEN_ALWAYS;
	switch (mode) {
		case SYS_CREATE_ALWAYS: os_mode = CREATE_ALWAYS; break;
		case SYS_CREATE_NEW: os_mode = CREATE_NEW; break;
		case SYS_OPEN_ALWAYS: os_mode = OPEN_ALWAYS; break;
		case SYS_OPEN_EXISTING: os_mode = OPEN_EXISTING; break;
		case SYS_TRUNCATE_EXISTING: os_mode = TRUNCATE_EXISTING; break;
	}

	FILE_HANDLE handle = CreateFileA(name, os_flags, os_sharing_flags, NULL, os_mode, 0, NULL);
	return handle;
}

int sys_get_file_times(FILE_HANDLE file, FILE_TIMES *times) {
	int result = GetFileTime(file, &times->create, &times->access, &times->write);
	//	FILETIME create, access, write;
	//	int result = GetFileTime(file, &create, &access, &write);
	//	times->create = (FILE_TIME) { create.dwLowDateTime, create.dwHighDateTime };
	//	times->access = (FILE_TIME) { access.dwLowDateTime, access.dwHighDateTime };
	//	times->write = (FILE_TIME) { write.dwLowDateTime, write.dwHighDateTime };
	return result;
}

void sys_file_time_to_system_time(FILE_TIME *filetime, SYSTEM_TIME *systimeout) {
	FileTimeToSystemTime(filetime, systimeout);
}


#if 0
int lib_core_shell(elf_State *R) {
	char *verb = elf_get_text_arg(R,0);
	char *file = elf_get_text_arg(R,1);
	char *args = elf_get_text_arg(R,2);

	int success = (INT_PTR) ShellExecute(NULL,verb,file,args,NULL,10) > 32;
	elf_push_int(R,success);
	return 1;
}





int core_lib_get_disk_info(elf_State *R) {
	elf_Table *info = elf_new_table(R);
#if 0
	DWORD SectorsPerCluster;
	DWORD BytesPerSector;
	DWORD NumberOfFreeClusters;
	DWORD TotalNumberOfClusters;
	GetDiskFreeSpaceA(elf_get_text_arg(R,0),&SectorsPerCluster,&BytesPerSector,&NumberOfFreeClusters,&TotalNumberOfClusters);
	elf_tsets_int(info,elf_alloc_string(R,"SectorsPerCluster"),SectorsPerCluster);
	elf_tsets_int(info,elf_alloc_string(R,"BytesPerSector"),BytesPerSector);
	elf_tsets_int(info,elf_alloc_string(R,"NumberOfFreeClusters"),NumberOfFreeClusters);
	elf_tsets_int(info,elf_alloc_string(R,"TotalNumberOfClusters"),TotalNumberOfClusters);
#else
	elf_debug_log("this function is not implemented for this platform");
#endif
	return 1;
}


int core_lib_list_volumes(elf_State *R) {
	elf_Table *list = elf_new_table(R); /* <- */
#if 0
	char buffer[MAX_PATH];
	HANDLE handle = FindFirstVolumeA(buffer,MAX_PATH);

	elf_String *name = 0;
	if (handle != INVALID_HANDLE_VALUE) do {

		elf_Table *volume = elf_new_table(R);
		name = elf_new_string(R,buffer);

		elf_tsets_tab(list,name,volume);

		elf_tsets_str(volume,elf_alloc_string(R,"name"),name);

		elf_Table *path_names = elf_new_table(R);
		elf_tsets_tab(volume,elf_alloc_string(R,"path_names"),path_names);

		if (GetVolumePathNamesForVolumeNameA(name->text,buffer,MAX_PATH,NULL)) {
			char *cursor = buffer;
			while (*cursor != '\0') {
				elf_array_add_raw(path_names,VALUE_STRING(elf_alloc_string(R,buffer)));
				cursor += strlen(cursor) + 1;
			}
		}
	} while(FindNextVolumeA(handle,buffer,MAX_PATH));
	FindVolumeClose(handle);
#endif

	elf_push_table_raw(R,list); /* <- */
	return 1;
}
#endif






bool sys_debugger() {
	DebugBreak();
	return 1;
}

// todo: make this legit, it should be the other way around!
#include "logging.c"
void sys_console_print(int type, char *message) {
	elf_log(type,"%s",message);
}


int sys_get_last_error() {
	return GetLastError();
}


void sys_get_error_msg(int error, char *buf, int len) {
	FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM,0x00,error,LANG_USER_DEFAULT,buf,len,NULL);
}


void *sys_virtual_alloc(elf_i64 length) {
	return VirtualAlloc(NULL,length,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);
}

void sys_virtual_dealloc(void *memory) {
	VirtualFree(memory,0,MEM_RELEASE);
}


void sys_sleep(elf_Int ms) {
	Sleep((DWORD) ms);
}


elf_Int sys_get_clock_freq() {
	LARGE_INTEGER large_integer;
	QueryPerformanceFrequency(&large_integer);
	return large_integer.QuadPart;
}


elf_Int sys_get_clock_time() {
	LARGE_INTEGER large_integer;
	QueryPerformanceCounter(&large_integer);
	return large_integer.QuadPart;
}


int sys_get_my_name(int length, char *buffer) {
	return GetModuleFileName(NULL,buffer,length);
}


int sys_get_my_pid() {
	return GetCurrentProcessId();
}


int sys_get_work_dir(int length, char *buffer) {
	return GetCurrentDirectory(length,buffer);
}


int sys_set_work_dir(char *buffer) {
	return SetCurrentDirectory(buffer);
}


elf_Handle sys_load_dll(char const *name) {
	return (elf_Handle) LoadLibraryA(name);
}


void *sys_get_dll_fn(elf_Handle dll, char const *name) {
	return (void *) GetProcAddress(dll,name);
}

static inline void convfiledata(FILE_VISITOR *visitor, WIN32_FIND_DATAA *info) {
	visitor->type = FILE_TYPE_FILE;
	if (info->dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) {
		visitor->type = FILE_TYPE_SYMLINK;
	} else if (info->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
		visitor->type = FILE_TYPE_FOLDER;
	}
	visitor->size = info->nFileSizeLow;
	ASSERT(sizeof(visitor->name) >= sizeof(info->cFileName));
	CopyMemory(visitor->name, info->cFileName, sizeof(info->cFileName));
}

int sys_open_directory(FILE_VISITOR *visitor, char *const path) {
	WIN32_FIND_DATAA info;
	HANDLE hand = FindFirstFileA(elf_tpf("%s\\*", path), &info);
	visitor->hand = hand;

	convfiledata(visitor, &info);
	return hand != INVALID_HANDLE_VALUE;
}

int sys_read_directory(FILE_VISITOR *visitor) {
	WIN32_FIND_DATAA info;
	int result = FindNextFileA((HANDLE) visitor->hand, &info);
	convfiledata(visitor, &info);
	return result;
}


int sys_create_process(char const *file, char const *args) {
	STARTUPINFO si = {sizeof(si)};
	PROCESS_INFORMATION pi = {0};
	int result = CreateProcess(file,(char*)args,NULL,NULL,FALSE,0,NULL,NULL,&si,&pi);
	CloseHandle(pi.hProcess);
	CloseHandle(pi.hThread);
	return result;
}
