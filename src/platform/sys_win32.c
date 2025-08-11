//
// See Copyright Notice In elf.h
//

#pragma comment(lib,"user32")
#pragma comment(lib,"Ws2_32")
#define WIN32_LEAN_AND_MEAN

#include "winconfig.h"

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

FILE_HANDLE sys_get_std_file(int std) {
	switch (std) {
		case SYS_STD_OUTPUT: return GetStdHandle(STD_OUTPUT_HANDLE);
		case SYS_STD_INPUT:  return GetStdHandle(STD_INPUT_HANDLE);
		case SYS_STD_ERROR:  return GetStdHandle(STD_ERROR_HANDLE);
	}
	return 0;
}



unsigned int sys_read_console(FILE_HANDLE file, char *buf, unsigned int zbuf) {
	DWORD read;
	ReadConsole(GetStdHandle(STD_INPUT_HANDLE), buf, zbuf, &read, NULL);
	return read;
}



unsigned int sys_read_file(FILE_HANDLE file, char *buf, unsigned int zbuf) {
	DWORD read = 0;
	ReadFile(file, buf, zbuf, &read, NULL);
	return read;
}

unsigned int sys_write_file(FILE_HANDLE file, char *buf, unsigned int zbuf) {
	DWORD wrote = 0;
	WriteFile(file, buf, zbuf, &wrote, NULL);
	return wrote;
}

unsigned int sys_size_file(FILE_HANDLE file) {
	DWORD zfile = GetFileSize(file, NULL);
	return zfile;
}


int sys_create_directory(const char *path) {
	return CreateDirectory(path, NULL);
}


bool sys_delete_file(const char *path) {
	return DeleteFile(path);
}

void sys_close_file(FILE_HANDLE file) {
	CloseHandle(file);
}

int sys_get_file_cursor(FILE_HANDLE file) {
	return SetFilePointer(file, 0, 0, FILE_CURRENT);
}

int sys_set_file_cursor(FILE_HANDLE file, int cursor) {
	return SetFilePointer(file, cursor, 0, FILE_CURRENT);
}

// todo: support temporary files
FILE_HANDLE sys_open_file(const char *name, int flags, int mode) {

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

	FILE_HANDLE hfile = CreateFileA(name, os_flags, os_sharing_flags, NULL, os_mode, 0, NULL);
	return hfile;
}

int sys_time_file(FILE_HANDLE file, FILE_TIMES *times) {
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
	char *verb = f_checktext(R,0);
	char *file = f_checktext(R,1);
	char *args = f_checktext(R,2);

	int success = (INT_PTR) ShellExecute(NULL,verb,file,args,NULL,10) > 32;
	elf_pushint(R,success);
	return 1;
}





int core_lib_get_disk_info(elf_State *R) {
	elf_Table *info = elf_new_table(R);
#if 0
	DWORD SectorsPerCluster;
	DWORD BytesPerSector;
	DWORD NumberOfFreeClusters;
	DWORD TotalNumberOfClusters;
	GetDiskFreeSpaceA(f_checktext(R,0),&SectorsPerCluster,&BytesPerSector,&NumberOfFreeClusters,&TotalNumberOfClusters);
	elf_tsets_int(info,elf_alloc_string(R,"SectorsPerCluster"),SectorsPerCluster);
	elf_tsets_int(info,elf_alloc_string(R,"BytesPerSector"),BytesPerSector);
	elf_tsets_int(info,elf_alloc_string(R,"NumberOfFreeClusters"),NumberOfFreeClusters);
	elf_tsets_int(info,elf_alloc_string(R,"TotalNumberOfClusters"),TotalNumberOfClusters);
#else
	elf_ldebug("this function is not implemented for this platform");
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
				elf_raw_array_add(path_names,VALUE_STRING(elf_alloc_string(R,buffer)));
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


void sys_sleep(elf_Integer ms) {
	Sleep((DWORD) ms);
}


elf_Integer sys_get_performance_counter_frequency() {
	LARGE_INTEGER large_integer;
	QueryPerformanceFrequency(&large_integer);
	return large_integer.QuadPart;
}


elf_Integer sys_get_performance_counter() {
	LARGE_INTEGER large_integer;
	QueryPerformanceCounter(&large_integer);
	return large_integer.QuadPart;
}

void sys_exit_this_process(int errorcode) {
	ExitProcess(errorcode);
}

//	int sys_get_my_name(int length, char *buffer) {
//		return GetModuleFileName(NULL,buffer,length);
//	}

int sys_get_this_process_id() {
	return GetCurrentProcessId();
}


int sys_get_work_dir(char *buf, int bufsize) {
	return GetCurrentDirectory(bufsize, buf);
}


int sys_set_work_dir(char *buf) {
	return SetCurrentDirectory(buf);
}


elf_Handle sys_load_dll(char const *name) {
	return (elf_Handle) LoadLibraryA(name);
}


void *sys_get_dll_fn(elf_Handle dll, char const *name) {
	return (void *) GetProcAddress((HMODULE) dll, name);
}

static inline void pushfiledata(FILE_VISITOR *visitor, WIN32_FIND_DATAA *info) {
	visitor->type = FILE_TYPE_FILE;
	if (info->dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) {
		visitor->type = FILE_TYPE_SYMLINK;
	} else if (info->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
		visitor->type = FILE_TYPE_FOLDER;
	}
	visitor->size = info->nFileSizeLow;
	pushpath(&visitor->pb, info->cFileName);
}


void sys_find_close(FILE_HANDLE hand) {
	FindClose(hand);
}

FILE_HANDLE sys_find_first_file(FILE_VISITOR *visitor) {

	// insert some temporary stuff for windows path matching
	visitor->pb.path[visitor->pb.pcur ++] = '\\';
	visitor->pb.path[visitor->pb.pcur ++] = '*';
	visitor->pb.path[visitor->pb.pcur   ] = '\0';

	WIN32_FIND_DATAA info;
	FILE_HANDLE hand = FindFirstFileA(visitor->pb.path, &info);

	// pop what we inserted
	visitor->pb.pcur -= 2;
	visitor->pb.path[visitor->pb.pcur] = '\0';

	int result = hand != INVALID_HANDLE_VALUE;
	if (result) {
		pushfiledata(visitor, &info);
	}
	return result ? hand : 0;
}

int sys_find_next_file(FILE_HANDLE hand, FILE_VISITOR *visitor) {
	// pull the path from before
	pullpath(&visitor->pb);

	WIN32_FIND_DATAA info;
	int noerr = FindNextFileA(hand, &info);
	if (noerr) {
		pushfiledata(visitor, &info);
	}
	return noerr;
}

elf_Handle sys_create_process(char const *file, char const *args) {
	STARTUPINFO startupinfo = {sizeof(startupinfo)};
	PROCESS_INFORMATION processinfo = {0};

	CreateProcess(file,(char*)args,NULL,NULL,FALSE,0,NULL,NULL,&startupinfo,&processinfo);
	WaitForSingleObject(processinfo.hProcess, INFINITE);
	CloseHandle(processinfo.hProcess);
	CloseHandle(processinfo.hThread);
	return 0;
}
