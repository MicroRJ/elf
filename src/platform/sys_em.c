//
// See Copyright Notice In elf.h
//


// emscripten includes
#include <sys/types.h>
#include <dlfcn.h>
#include <dirent.h>



bool sys_debugger() {
	emscripten_debugger();
	return 1;
}

void sys_console_print(int type, char *message) {
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
emscripten_log(type, message);
}


int sys_get_last_error() {
	return 0;
}


void sys_get_error_msg(int error, char *buf, int len) {
}

void *sys_virtual_alloc(elf_i64 length) {
	return 0;
}

void sys_virtual_dealloc(void *memory) {
	return 0;
}


void sys_sleep(elf_Int ms) {
	emscripten_sleep(ms);
}


elf_Int sys_get_clock_freq() {
	/* todo: where does it say this */
	return 1000;
}


elf_Int sys_get_clock_time() {
	return emscripten_get_now();
}


int sys_get_my_name(int length, char *buffer) {
	return 0;
}


int sys_get_my_pid() {
	return 0;
}


int sys_get_work_dir(int length, char *buffer) {
	return 0;
}


int sys_set_work_dir(char *buffer) {
	return !chdir(buffer);
}

// typedef void (*em_dlopen_callback)(void* handle, void* user_data);
// void emscripten_dlopen(const char *filename, int flags, void* user_data, em_dlopen_callback onsuccess, em_arg_callback_func onerror);


elf_Handle sys_load_dll(char const *name) {
#if 0
	em_promise_t promise = emscripten_dlopen_promise(name,RTLD_LAZY);
	em_settled_result_t result = emscripten_promise_await(promise);
	emscripten_promise_destroy(promise);
	return (elf_Handle) result.value;
#endif
	void *handle = dlopen(name,RTLD_LAZY);

	if (handle == 0) {
		sys_console_print(LOG_KERROR,"the following is a system error:");
		sys_console_print(LOG_KERROR, dlerror());
		sys_console_print(LOG_KERROR, "end");
	}
	return (elf_Handle) handle;
}


void *sys_get_dll_fn(elf_Handle dll, char const *name) {
	return (void *) dlsym(dll,name);
}





static inline void em_dirent_to_file_data(FILE_VISITOR *visitor, dirent *info) {
	int isdir = () != 0;

	if (is_file_name_empty(info->d_name)) {
		visitor->type = FILE_TYPE_SYMLINK;
	} else if (info->d_type & DT_DIR) {
		visitor->type = FILE_TYPE_FOLDER;
	} else {
		visitor->type = FILE_TYPE_FILE;
	}

	CopyMemory(visitor->name, info->d_name, sizeof(info->d_name));
}

int sys_open_directory(FILE_VISITOR *visitor, char *const path) {

	DIR *dir = opendir(path);
	visitor->hand = dir;

	struct dirent *entry = 0;

	if (dir) {
		entry = readdir(dir);
		if (entry) {
			em_dirent_to_file_data(visitor, entry);
		}
	}

	return entry != 0;
}

int sys_read_directory(FILE_VISITOR *visitor) {
	struct dirent *entry = readdir(dir);
	if (entry) {
		em_dirent_to_file_data(visitor, entry);
	}
	return entry != 0;
}



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
			bool isdir = (entry->d_type & DT_DIR) != 0;
			elf_Value *top = GET_TOP(R);

			elf_String *name = elf_new_string(R,entry->d_name);
			elf_String *path = elf_new_string(R,elf_tpf("%s/%s",dir->c,entry->d_name));
			elf_StackId base = elf_push_closure_raw(R,cls);
			elf_Table *file = elf_new_table(R);

			elf_tsets_str(file,elf_new_string(R,"name"),name);
			elf_tsets_str(file,elf_new_string(R,"path"),path);
			elf_tsets_int(file,elf_new_string(R,"isdir"),isdir);
			int r = elf_call(R,base,1,1);
			if ((r > 0) && isdir && elf_get_intarg(R,base)) {
				core_lib_enumerate_folder_(R,path,cls);
			}
			SET_TOP(R,top);
		}
		closedir(dirfd);

#endif