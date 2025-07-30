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


#if 0
int lib_core_shell(elf_State *R) {
	char *verb = elf_get_text(R,0);
	char *file = elf_get_text(R,1);
	char *args = elf_get_text(R,2);

	int success = (INT_PTR) ShellExecute(NULL,verb,file,args,NULL,10) > 32;
	elf_push_int(R,success);
	return 1;
}


int core_lib_exec(elf_State *R) {
	char *cline=elf_get_text(R,0);
	int result=sys_exec(0,cline);
	elf_push_int(R,result);
	return 1;
}



int core_lib_get_disk_info(elf_State *R) {
	elf_Table *info = elf_new_table(R);
#if 0
	DWORD SectorsPerCluster;
	DWORD BytesPerSector;
	DWORD NumberOfFreeClusters;
	DWORD TotalNumberOfClusters;
	GetDiskFreeSpaceA(elf_get_text(R,0),&SectorsPerCluster,&BytesPerSector,&NumberOfFreeClusters,&TotalNumberOfClusters);
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
				elf_array_add(path_names,VALUE_STRING(elf_alloc_string(R,buffer)));
				cursor += strlen(cursor) + 1;
			}
		}
	} while(FindNextVolumeA(handle,buffer,MAX_PATH));
	FindVolumeClose(handle);
#endif

	elf_push_table(R,list); /* <- */
	return 1;
}
#endif






bool sys_debugger() {
	DebugBreak();
	return 1;
}

// todo: make this legit, this depends on the subsystem!
void sys_console_print(int type, char *message) {
	elf_log(type,"%s",message);
}


int sys_get_last_error() {
	return GetLastError();
}


void sys_get_error_msg(int error, char *buf, int len) {
	if (error == 0) error = GetLastError();
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



static int _sym_link(char const *name) {
	while (*name == '.') ++ name;
	return *name == 0;
}


static inline void win32_find_data_to_file_data(FILE_VISITOR *visitor, WIN32_FIND_DATAA *info) {
	if (_sym_link(info->cFileName)) {
		visitor->type = FILE_TYPE_SYMLINK;
	} else if (info->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
		visitor->type = FILE_TYPE_FOLDER;
	} else {
		visitor->type = FILE_TYPE_FILE;
	}
	visitor->size = info->nFileSizeLow;
	CopyMemory(visitor->name, info->cFileName, sizeof(info->cFileName));
}

int sys_opendir(FILE_VISITOR *visitor, char *const path) {
	WIN32_FIND_DATAA info;
	HANDLE hand = FindFirstFileA(elf_tpf("%s\\*", path), &info);

	win32_find_data_to_file_data(visitor, &info);
	visitor->hand = hand;
	return hand != INVALID_HANDLE_VALUE;
}

int sys_readdir(FILE_VISITOR *visitor) {
	WIN32_FIND_DATAA info;
	int result = FindNextFileA((HANDLE) visitor->hand, &info);
	win32_find_data_to_file_data(visitor, &info);
	return result;
}


int sys_exec(char const *file, char const *args) {
	STARTUPINFO si = {sizeof(si)};
	PROCESS_INFORMATION pi = {0};
	int result = CreateProcess(file,(char*)args,NULL,NULL,FALSE,0,NULL,NULL,&si,&pi);
	CloseHandle(pi.hProcess);
	CloseHandle(pi.hThread);
	return result;
}
