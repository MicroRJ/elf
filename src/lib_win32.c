/*
** See Copyright Notice In elf.h
** lib_win32.c
** implementation of some of the core lib for windows
*/

#pragma comment(lib,"user32")
#pragma comment(lib,"Ws2_32")
#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <Windowsx.h>
#include <Winsock2.h>
#include <ws2tcpip.h>
#include   <ws2def.h>
#include <shellapi.h>

// typedef void (*em_dlopen_callback)(void* handle, void* user_data);
// void emscripten_dlopen(const char *filename, int flags, void* user_data, em_dlopen_callback onsuccess, em_arg_callback_func onerror);
int core_lib_get_dll_fn(elf_State *S) {
	elf_Handle lib = elf_get_sysobj(S,0);
	char *name = elf_get_text(S,1);
	elf_Function fn = (elf_Function) sys_get_dll_fn(lib,name);

	if (fn != 0) elf_push_proc(S,fn);
	else elf_push_nil(S);
	return 1;
}


int core_lib_load_dll(elf_State *R) {
	char *file = elf_get_text(R,0);
	elf_Handle lib = sys_load_dll(file);

	if (lib != 0) elf_add_sys(R,lib);
	else elf_push_nil(R);
	return 1;
}

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
				elf_array_add(path_names,VSTR(elf_alloc_string(R,buffer)));
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


static int _sym_link(char const *name) {
	while (*name == '.') ++ name;
	return *name == 0;
}

static int core_lib_list_folder(elf_State *S) {

	char *base = elf_get_text(S,0);

	elf_String *folder_s = elf_new_string(S,"folder");
	elf_String *file_s = elf_new_string(S,"file");
	elf_String *name_s = elf_new_string(S,"name");
	elf_String *path_s = elf_new_string(S,"path");
	elf_String *type_s = elf_new_string(S,"type");
	elf_String *size_s = elf_new_string(S,"size");

	elf_Table *array = elf_new_table(S);
	WIN32_FIND_DATAA info;
	HANDLE search = FindFirstFileA(elf_tpf("%s\\*",base),&info);
	if (search!=INVALID_HANDLE_VALUE) do {
		if (_sym_link(info.cFileName)) continue;

		char *name = info.cFileName;
		int type = info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY;
		int size = info.nFileSizeLow;

		elf_Table *file = elf_new_table(S);
		elf_tsets_str(file,name_s,elf_new_string(S,name));
		elf_tsets_str(file,path_s,elf_new_string(S,elf_tpf("%s\\%s",base,name)));
		elf_tsets_str(file,type_s,type?folder_s:file_s);
		elf_tsets_int(file,size_s,size);
		elf_array_add(array,VTAB(file));

	} while (FindNextFileA(search,&info));

	elf_push_table(S,array);
	return 1;
}
