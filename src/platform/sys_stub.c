//
// See Copyright Notice In elf.h
//

#include <sys/types.h>
#include <dlfcn.h>
#include <dirent.h>
#include <limits.h>

b32 elf_platform_debug_break() {
	emscripten_debugger();
	return 1;
}

void elf_platform_console_print(int type, char *message) {
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

void elf_platform_enable_console_colors(void)
{
}


int elf_platform_last_error() {
	return 0;
}


void elf_platform_error_message(int error, char *buf, int len) {
}

void *elf_platform_virtual_alloc(elf_i64 length) {
	return 0;
}

void elf_platform_virtual_free(void *memory) {
	return 0;
}


void elf_platform_sleep(elf_Integer ms) {
	emscripten_sleep(ms);
}


elf_i64 elf_platform_counter_frequency() {
	/* todo: where does it say this */
	return 1000;
}


elf_i64 elf_platform_counter() {
	return emscripten_get_now();
}


int sys_get_my_name(int length, char *buffer) {
	return 0;
}


int elf_platform_process_id() {
	return 0;
}


int elf_platform_work_dir(char *buffer, int length) {
	(void)buffer;
	(void)length;
	return 0;
}


int elf_platform_set_work_dir(const char *buffer) {
	return !chdir(buffer);
}

// typedef void (*em_dlopen_callback)(void* handle, void* user_data);
// void emscripten_dlopen(const char *filename, int flags, void* user_data, em_dlopen_callback onsuccess, em_arg_callback_func onerror);


elf_PlatformFile elf_platform_load_dll(char const *name) {
#if 0
	em_promise_t promise = emscripten_dlopen_promise(name,RTLD_LAZY);
	em_settled_result_t result = emscripten_promise_await(promise);
	emscripten_promise_destroy(promise);
	return (elf_PlatformFile) result.value;
#endif
	void *handle = dlopen(name,RTLD_LAZY);

	if (handle == 0) {
		elf_platform_console_print(LOG_KERROR,"the following is a system error:");
		elf_platform_console_print(LOG_KERROR, dlerror());
		elf_platform_console_print(LOG_KERROR, "end");
	}
	return (elf_PlatformFile) handle;
}


void *elf_platform_dll_symbol(elf_PlatformFile dll, char const *name) {
        return (void *) dlsym(dll,name);
}

elf_PlatformProcessResult elf_platform_run_process(const char *command_line, struct elf_Arena *standard_output,
                                   struct elf_Arena *standard_error)
{
        (void)command_line;
        (void)standard_output;
        (void)standard_error;
        return (elf_PlatformProcessResult){.exit_code = -1, .error_code = -1};
}

static b32 sys_is_virtual_path(const char *name)
{
	return (name[0] == '.' && name[1] == 0)
	|| (name[0] == '.' && name[1] == '.' && name[2] == 0);
}

#define ELF_FS_MAX_RECURSION 32

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

static b32 sys_collect_paths(elf_State *S, char *path, u32 path_size,
        u32 recursion_level, elf_i32 output, b32 call_each, u32 *count)
{
	b32 separator = path[path_size - 1] != '/';
	u32 child_offset = path_size + separator;
	if (child_offset >= PATH_MAX) return false;
	path[path_size] = 0;
	DIR *handle = opendir(path);
	if (!handle) return false;
	if (separator) path[path_size] = '/';

	struct dirent *entry;
	while ((entry = readdir(handle)) != 0)
	{
		const char *name = entry->d_name;
		if (sys_is_virtual_path(name)) continue;
		u32 name_size = (u32)strlen(name);
		if (child_offset + name_size >= PATH_MAX) continue;
		memcpy(path + child_offset, name, name_size);
		u32 child_size = child_offset + name_size;
                if (call_each)
                {
                        elf_push_value(S, output);
                        elf_push_nil(S);
                        elf_push_str(S, path, (int)child_size);
                        elf_call(S, 2, 0);
                        ++*count;
                }
                else
                {
                        elf_push_str(S, path, (int)child_size);
                        elf_append(S, output);
                }

		if (recursion_level && entry->d_type == DT_DIR)
		{
                        sys_collect_paths(S, path, child_size, recursion_level - 1,
                                output, call_each, count);
		}
	}

	closedir(handle);
	return true;
}

ELF_FUNCTION(elf_platform_fs_get_paths)
{
	(void)nrets;
	if (nargs < 1 || nargs > 3)
	{
		elf_push_nil(S);
		return 1;
	}

	elf_StrSlice root = {".", 1};
	if ((nargs >= 2 && !elf_to_str(S, 1, &root))
	|| root.size == 0 || root.size >= PATH_MAX)
	{
		elf_push_nil(S);
		return 1;
	}

	elf_Integer recursion_level = 0;
	if ((nargs == 3 && !elf_to_int(S, 2, &recursion_level))
	|| recursion_level < 0 || recursion_level > ELF_FS_MAX_RECURSION)
	{
		elf_push_nil(S);
		return 1;
	}

	elf_new_table(S);
	elf_i32 result = elf_abs_index(S, -1);
	elf_Scratch scratch = elf_begin_scratch();
	char *path = elf_arena_push(scratch.arena, PATH_MAX);
	memcpy(path, root.data, root.size);
        u32 count = 0;
        b32 success = sys_collect_paths(S, path, (u32)root.size,
                (u32)recursion_level, result, false, &count);
	elf_end_scratch(scratch);
	if (!success)
	{
		elf_pop(S, 1);
		elf_push_nil(S);
	}
        return 1;
}

ELF_FUNCTION(elf_platform_fs_for_each_path)
{
        (void)nrets;
        if (nargs < 2 || nargs > 4)
        {
                elf_push_nil(S);
                return 1;
        }

        elf_StrSlice root = {".", 1};
        elf_Integer recursion_level = 0;
        elf_i32 callback = 1;
        if (nargs >= 3)
        {
                if (!elf_to_str(S, 1, &root))
                {
                        elf_push_nil(S);
                        return 1;
                }
                callback = 2;
        }
        if (nargs == 4)
        {
                if (!elf_to_int(S, 2, &recursion_level))
                {
                        elf_push_nil(S);
                        return 1;
                }
                callback = 3;
        }
        if (root.size == 0 || root.size >= PATH_MAX
        || recursion_level < 0 || recursion_level > ELF_FS_MAX_RECURSION
        || !elf_is_callable(S, callback))
        {
                elf_push_nil(S);
                return 1;
        }

        callback = elf_abs_index(S, callback);
        elf_Scratch scratch = elf_begin_scratch();
        char *path = elf_arena_push(scratch.arena, PATH_MAX);
        memcpy(path, root.data, root.size);
        u32 count = 0;
        b32 success = sys_collect_paths(S, path, (u32)root.size,
                (u32)recursion_level, callback, true, &count);
        elf_end_scratch(scratch);
        if (success) elf_push_int(S, count);
        else elf_push_nil(S);
        return 1;
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
			b32 isdir = (entry->d_type & DT_DIR) != 0;
			elf_Value *top = GET_TOP(R);

			elf_String *name = elf_atom_from_data(R,entry->d_name);
			elf_String *path = elf_atom_from_data(R,tpf("%s/%s",dir->c,entry->d_name));
			elf_StackId base = elf_push_closure_raw(R,cls);
			elf_Table *file = elf_new_table(R);

			elf_tsets_str(file,elf_atom_from_data(R,"name"),name);
			elf_tsets_str(file,elf_atom_from_data(R,"path"),path);
			elf_tsets_int(file,elf_atom_from_data(R,"isdir"),isdir);
			int r = elf_call(R,base,1,1);
			if ((r > 0) && isdir && f_checkint(R,base)) {
				core_lib_enumerate_folder_(R,path,cls);
			}
			SET_TOP(R,top);
		}
		closedir(dirfd);

#endif
