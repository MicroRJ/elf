//
// See Copyright Notice In elf.h
//

#pragma comment(lib,"user32")
#pragma comment(lib,"Ws2_32")
#define WIN32_LEAN_AND_MEAN

// todo: should probably just define the functions I want to use instead!
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

#include <windows.h>
#include <Windowsx.h>
#include <shellapi.h>

#define FILE_HANDLE HANDLE
#define FILE_TIME FILETIME
#define SYSTEM_TIME SYSTEMTIME


#include "elf.h"
#include "base.h"
#include "system.h"



STATIC_ASSERT(sizeof(_FILE_HANDLE) >= sizeof(FILE_HANDLE));



char *sys_get_cmd_line() {
	return GetCommandLineA();
}




FILE_HANDLE sys_get_std_file(int std) {
	switch (std) {
		case SYS_STD_OUTPUT: return GetStdHandle(STD_OUTPUT_HANDLE);
		case SYS_STD_INPUT:  return GetStdHandle(STD_INPUT_HANDLE);
		case SYS_STD_ERROR:  return GetStdHandle(STD_ERROR_HANDLE);
	}
	return 0;
}

static void sys_enable_console_colors_for_file(FILE_HANDLE file)
{
	DWORD mode = 0;
	if (file && file != INVALID_HANDLE_VALUE && GetConsoleMode(file, &mode))
	{
		mode |= ENABLE_PROCESSED_OUTPUT | ENABLE_VIRTUAL_TERMINAL_PROCESSING;
		SetConsoleMode(file, mode);
	}
}

void sys_enable_console_colors(void)
{
	sys_enable_console_colors_for_file(GetStdHandle(STD_OUTPUT_HANDLE));
	sys_enable_console_colors_for_file(GetStdHandle(STD_ERROR_HANDLE));
}

unsigned int sys_read_console(FILE_HANDLE file, char *buf, unsigned int zbuf) {
	DWORD read;
	ReadConsole(GetStdHandle(STD_INPUT_HANDLE), buf, zbuf, &read, NULL);
	return read;
}



elf_i64 elf_platform_read_file(FILE_HANDLE file, void *buf, elf_i64 zbuf) {
	DWORD read = 0;
	ReadFile(file, buf, zbuf, &read, NULL);
	return read;
}

elf_i64 sys_write_file(FILE_HANDLE file, void *buf, elf_i64 zbuf) {
	DWORD wrote = 0;
	WriteFile(file, buf, zbuf, &wrote, NULL);
	return wrote;
}

elf_i64 elf_platform_get_file_size(FILE_HANDLE file) {
	DWORD zfile = GetFileSize(file, NULL);
	return zfile;
}


int sys_make_dir(const char *path) {
	return CreateDirectory(path, NULL);
}


bool sys_delete_file(const char *path) {
	return DeleteFile(path);
}

void elf_platform_close_file(FILE_HANDLE file) {
	CloseHandle(file);
}

void sys_flush_file(FILE_HANDLE file) {
	FlushFileBuffers(file);
}

elf_i64 sys_move_file_cursor(FILE_HANDLE file, int relativeto, elf_i64 dist) {
	if (relativeto == SYS_END) {
		return SetFilePointer(file, dist, 0, FILE_END);
	} else if (relativeto == SYS_BEGIN) {
		return SetFilePointer(file, dist, 0, FILE_BEGIN);
	} else {
		return SetFilePointer(file, dist, 0, FILE_CURRENT);
	}
}

// todo: support temporary files
FILE_HANDLE elf_platform_access_file(const char *name, int flags, int mode) {

	int os_flags = 0;
	if (flags & SYS_OPEN_READ) os_flags |= GENERIC_READ;
	if (flags & SYS_OPEN_WRITE) os_flags |= GENERIC_WRITE;
	if (flags & SYS_OPEN_EXECUTE) os_flags |= GENERIC_EXECUTE;

	int os_sharing_flags = 0;
	if (flags & SYS_SHARE_READ) os_sharing_flags |= FILE_SHARE_READ;
	if (flags & SYS_SHARE_WRITE) os_sharing_flags |= FILE_SHARE_WRITE;

	int os_misc_flags = 0;
	if (flags & SYS_NO_BUFFERING) os_sharing_flags |= FILE_FLAG_NO_BUFFERING;

	int os_mode = OPEN_ALWAYS;
	switch (mode) {
		case SYS_CREATE_ALWAYS: os_mode = CREATE_ALWAYS; break;
		case SYS_CREATE_NEW: os_mode = CREATE_NEW; break;
		case SYS_OPEN_ALWAYS: os_mode = OPEN_ALWAYS; break;
		case SYS_OPEN_EXISTING: os_mode = OPEN_EXISTING; break;
		case SYS_TRUNCATE_EXISTING: os_mode = TRUNCATE_EXISTING; break;
	}

	FILE_HANDLE hfile = CreateFileA(name, os_flags, os_sharing_flags, NULL, os_mode, 0, NULL);
	if (hfile == INVALID_HANDLE_VALUE) {
		return 0;
	}
	return hfile;
}



// todo: this depends on FILE_TIMES having the same structure!
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
	Scratch scratch = get_scratch();
	elf_StrSlice verb = elf_arg_str_copy(R, 0, scratch.arena);
	elf_StrSlice file = elf_arg_str_copy(R, 1, scratch.arena);
	elf_StrSlice args = elf_arg_str_copy(R, 2, scratch.arena);

	int success = (INT_PTR)ShellExecute(NULL, verb.data, file.data, args.data, NULL, 10) > 32;
	elf_push_int(R,success);
	end_scratch(scratch);
	return 1;
}





int core_lib_get_disk_info(elf_State *R) {
	elf_Table *info = elf_new_table(R);
#if 0
	DWORD SectorsPerCluster;
	DWORD BytesPerSector;
	DWORD NumberOfFreeClusters;
	DWORD TotalNumberOfClusters;
	Scratch scratch = get_scratch();
	elf_StrSlice path = elf_arg_str_copy(R, 0, scratch.arena);
	GetDiskFreeSpaceA(path.data,&SectorsPerCluster,&BytesPerSector,&NumberOfFreeClusters,&TotalNumberOfClusters);
	end_scratch(scratch);
	elf_tsets_int(info,elf_atom_from_data(R,"SectorsPerCluster"),SectorsPerCluster);
	elf_tsets_int(info,elf_atom_from_data(R,"BytesPerSector"),BytesPerSector);
	elf_tsets_int(info,elf_atom_from_data(R,"NumberOfFreeClusters"),NumberOfFreeClusters);
	elf_tsets_int(info,elf_atom_from_data(R,"TotalNumberOfClusters"),TotalNumberOfClusters);
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

	elf_Atom *name = 0;
	if (handle != INVALID_HANDLE_VALUE) do {

		elf_Table *volume = elf_new_table(R);
		name = elf_atom_from_data(R,buffer);

		elf_tsets_tab(list,name,volume);

		elf_tsets_str(volume,elf_atom_from_data(R,"name"),name);

		elf_Table *path_names = elf_new_table(R);
		elf_tsets_tab(volume,elf_atom_from_data(R,"path_names"),path_names);

		if (GetVolumePathNamesForVolumeNameA(name->data,buffer,MAX_PATH,NULL)) {
			char *cursor = buffer;
			while (*cursor != '\0') {
				elf_array_add(R, path_names, VALUE_ATOM(elf_atom_from_data(R,buffer)));
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
	// elf_log(type,"%s",message);
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

void sys_virtual_free(void *memory) {
	VirtualFree(memory,0,MEM_RELEASE);
}



void sys_sleep(elf_Integer ms) {
	Sleep((DWORD) ms);
}



elf_i64 sys_get_performance_counter_frequency() {
	LARGE_INTEGER large_integer;
	QueryPerformanceFrequency(&large_integer);
	return large_integer.QuadPart;
}



elf_i64 sys_get_performance_counter() {
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



int sys_set_work_dir(const char *buf) {
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
	path_push(&visitor->pb, info->cFileName);
}



void sys_find_close(FILE_HANDLE hand) {
	FindClose(hand);
}



FILE_HANDLE sys_find_first_file(FILE_VISITOR *visitor)
{
	path_push(&visitor->pb, "*");
	WIN32_FIND_DATAA info;
	FILE_HANDLE hand = FindFirstFileA(visitor->pb.path, &info);
	path_pop(&visitor->pb);

	int result = hand != INVALID_HANDLE_VALUE;
	if (result) {
		pushfiledata(visitor, &info);
	}
	return result ? hand : 0;
}



int sys_find_next_file(FILE_HANDLE hand, FILE_VISITOR *visitor) {
	// pull the path from before
	path_pop(&visitor->pb);

	WIN32_FIND_DATAA info;
	int noerr = FindNextFileA(hand, &info);
	if (noerr) {
		pushfiledata(visitor, &info);
	}
	return noerr;
}

// Todo, args must actually be writeable
elf_Handle sys_create_process(char const *file, char const *args)
{
	STARTUPINFO startupinfo = {sizeof(startupinfo)};
	PROCESS_INFORMATION processinfo = {0};

	CreateProcess(file,(char*)args,NULL,NULL,FALSE,0,NULL,NULL,&startupinfo,&processinfo);
	//	WaitForSingleObject(processinfo.hProcess, INFINITE);
	//	CloseHandle(processinfo.hProcess);
	//	CloseHandle(processinfo.hThread);
	return 0;
}
