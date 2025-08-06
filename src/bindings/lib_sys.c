//
// See Copyright Notice In elf.h
//


//
// TIMING
//

static inline elf_f64 get_performance_counter_elapsed_s(elf_i64 time) {
	return (sys_get_performance_counter() - time) / (elf_f64) sys_get_performance_counter_frequency();
}

ELF_FUNCTION(l_sys_get_performance_counter) {
	elf_push_int(S, sys_get_performance_counter());
	return 1;
}

ELF_FUNCTION(l_sys_get_performance_counter_frequency) {
	elf_push_int(S, sys_get_performance_counter_frequency());
	return 1;
}

ELF_FUNCTION(l_sys_get_performance_counter_elapsed_s) {
	ASSERT(elf_get_num_args(S) == 1);
	elf_i64 time = elf_get_intarg(S, 0);
	elf_push_num(S, get_performance_counter_elapsed_s(time));
	return 1;
}

ELF_FUNCTION(l_sys_get_performance_counter_elapsed_ms) {
	ASSERT(elf_get_num_args(S) == 1);
	elf_i64 time = elf_get_intarg(S, 0);
	elf_push_num(S, get_performance_counter_elapsed_s(time) * 1000);
	return 1;
}

ELF_FUNCTION(l_sys_sleep) {
	ASSERT(elf_get_num_args(S) >= 1);
	sys_sleep(elf_get_intarg(S, 0));
	return 0;
}

//
// files
//

ELF_FUNCTION(l_sys_get_file_name_from_path) {
	char *path = elf_get_text_arg(S, 0);
	elf_push_string(S, get_name_from_file_path(path));
	return 1;
}

ELF_FUNCTION(l_sys_create_directory) {
	char *path = elf_get_text_arg(S, 0);
	int result = sys_create_directory(path);
	elf_push_int(S, result);
	return 1;
}


ELF_FUNCTION(l_sys_get_file_times) {
	elf_Handle file = elf_get_sysarg(S, 0);

	FILE_TIMES times;
	sys_time_file(file, &times);

	elf_push_table(S);
	elf_push_string(S, "created");  elf_push_int(S, times.create.time); elf_table_set(S);
	elf_push_string(S, "access");   elf_push_int(S, times.access.time); elf_table_set(S);
	elf_push_string(S, "write");    elf_push_int(S, times.write.time);  elf_table_set(S);
	return 1;
}

ELF_FUNCTION(l_sys_file_time_to_system_time) {

	elf_Int time = elf_get_intarg(S, 0);
	FILE_TIME filetime = { .time = time };

	SYSTEM_TIME systemtime;
	sys_file_time_to_system_time(&filetime, &systemtime);

	elf_push_table(S);

	elf_push_string(S, "year");         elf_push_int(S, systemtime.year);           elf_table_set(S);
	elf_push_string(S, "month");        elf_push_int(S, systemtime.month);          elf_table_set(S);
	elf_push_string(S, "dayofweek");    elf_push_int(S, systemtime.dayofweek);      elf_table_set(S);
	elf_push_string(S, "day");          elf_push_int(S, systemtime.day);            elf_table_set(S);
	elf_push_string(S, "hour");         elf_push_int(S, systemtime.hour);           elf_table_set(S);
	elf_push_string(S, "minute");       elf_push_int(S, systemtime.minute);         elf_table_set(S);
	elf_push_string(S, "second");       elf_push_int(S, systemtime.second);         elf_table_set(S);
	elf_push_string(S, "milliseconds"); elf_push_int(S, systemtime.milliseconds);   elf_table_set(S);
	return 1;
}

//
// name: the name of the dynamic library in
// the file system
//
ELF_FUNCTION(l_sys_load_dll) {
	char *name = elf_get_text_arg(S, 0);

	elf_Handle lib = sys_load_dll(name);
	if (lib != 0) elf_push_handle(S, lib);
	else          elf_push_nil(S);
	return 1;
}

ELF_FUNCTION(l_sys_get_dll_fn) {
	elf_Handle lib = elf_get_sysarg(S, 0);
	char *name = elf_get_text_arg(S, 1);

	elf_Function fn = (elf_Function) sys_get_dll_fn(lib, name);
	if (fn != 0) elf_push_function(S,fn);
	else         elf_push_nil(S);
	return 1;
}

#if 0


static void filetreecompressed(FILE_VISITOR *visitor, File_Node *node, int recurse) {
	if (sys_open_directory(visitor)) do {
		if (visitor->type == FILE_TYPE_SYMLINK) continue;
		if (is_file_name_empty(visitor->name)) continue;

		int index = ARRAY_GROW(visitor->nodes, 1);
		File_Node *subnode = & visitor->nodes[index];
		subnode->nsub = 0;
		subnode->size = 0;

		if (visitor->type == FILE_TYPE_FOLDER) {
			pushpath(visitor, visitor->name);
			filetreecompressed(visitor, subnode, recurse - 1);
			pullpath(visitor);
		}

		node->size += subnode->size;
		node->nsub ++;
	} while (sys_read_directory(visitor));
}


static elf_StkInt filetree(elf_State *inter, char *path, int recurse, int *size) {

	elf_StkInt resstk = elf_push_table(inter);

	const char *type2s[] = {
		[FILE_TYPE_FILE] = "file",
		[FILE_TYPE_FOLDER] = "folder",
		[FILE_TYPE_SYMLINK] = "symlink",
	};

	elf_push_string(inter, "path");
	elf_push_string(inter, path);
	elf_table_set(inter);

	elf_push_string(inter, "type");
	elf_push_string(inter, type2s[visitor.type]);
	elf_table_set(inter);


	elf_push_table(inter);


	// todo: speed! we can pre-push all these strings and reference
	// them by stack address instead!
	FILE_VISITOR visitor;
	if (sys_open_directory(&visitor, path)) do {
		if (visitor.type == FILE_TYPE_SYMLINK) continue;
		if (is_file_name_empty(visitor.name)) continue;

		// todo: can we have our cake and eat it too please?
		char *childpath = malloc(1024);
		stbsp_snprintf(childpath, 1024, "%s\\%s", path, visitor.name);

		elf_push_table(inter);
		{

			if (visitor.type == FILE_TYPE_FOLDER) {
				if (recurse > 0) {
					elf_push_string(inter, "children");
					int childsize = 0;
					elf_StkInt child = filetree(inter, childpath, recurse - 1, &childsize);

					// todo:
					elf_push_string(inter, "parent");
					* inter->stack_ptr ++ = inter->stack[child];
					elf_table_set(inter);

					elf_table_set(inter);

					*size += childsize;
				}

				elf_push_string(inter, "size");
				elf_push_int(inter, visitor.size);
				elf_table_set(inter);
			} else {
				elf_push_string(inter, "size");
				elf_push_int(inter, visitor.size);
				elf_table_set(inter);

				*size += visitor.size;
			}

			free(childpath);
		}
		elf_array_add(inter);

	} while (sys_read_directory(&visitor));

	return resstk;
}
#endif


// NOTE: requires a table on the stack!
// TODO: use utility function to ensure this!
static void pathlist(elf_State *inter, FILE_VISITOR *visitor, int recurse) {
	elf_Handle dir = sys_open_directory(visitor);
	if (dir) do {
		if (visitor->type == FILE_TYPE_SYMLINK) goto nop;
		// todo: dedicated function, like points_to_file
		char *name = get_name_from_file_path(visitor->path);
		if (is_file_name_empty(name)) goto nop;

		if (visitor->type == FILE_TYPE_FILE) {
			elf_push_string(inter, visitor->path);
			elf_array_add(inter);
		} else if (visitor->type == FILE_TYPE_FOLDER) {
			if (recurse > 0) {
				pathlist(inter, visitor, recurse - 1);
			}
		}

		nop:;
		pullpath(visitor);
	} while (sys_read_directory(dir, visitor));
}

ELF_FUNCTION(l_sys_get_file_tree) {
	char *path = elf_get_text_arg(S,0);
	int recursion = 0;
	if (elf_get_num_args(S) >= 2) {
		recursion = elf_get_intarg(S,1);
	}
	// filetree(S, path, recursion);
	elf_push_nil(S);
	return 1;
}

ELF_FUNCTION(l_sys_get_path_list) {
	char *path = elf_get_text_arg(S,0);

	int recurse = 0;
	if (elf_get_num_args(S) >= 2) {
		recurse = elf_get_intarg(S,1);
	}
	elf_push_table(S);

	FILE_VISITOR *visi = calloc(1, sizeof(*visi));

	pushpath(visi, path);
	pathlist(S, visi, recurse);

	free(visi);
	return 1;
}

ELF_FUNCTION(l_sys_open_temp_file) {
	FILE *file = {0};
#if defined(PLATFORM_WEB)
	file = tmpfile();
#else
	tmpfile_s(&file);
#endif
	elf_push_handle(S,(elf_Handle)file);
	return 1;
}

ELF_FUNCTION(l_sys_delete_file) {
	elf_push_int(S, sys_delete_file(elf_get_text_arg(S, 0)));
	return 1;
}

ELF_FUNCTION(l_sys_open_file) {
	ASSERT(elf_get_num_args(S) == 2);

	char *name = elf_get_text_arg(S,0);
	char *text_flags = elf_get_text_arg(S,1);

	int flags;
	for (flags = 0; *text_flags; text_flags ++) {
		if (*text_flags == 'r') flags |= SYS_OPEN_READ;
		else if (*text_flags == 'w') flags |= SYS_OPEN_WRITE;
		else if (*text_flags == 'b') flags |= 0;
		else elf_error(S, NO_BYTE, "unrecognized flag");
	}
	int mode = SYS_OPEN_EXISTING;
	if (flags & SYS_OPEN_WRITE) {
		mode = SYS_CREATE_ALWAYS;
	}
	elf_Handle file = sys_open_file(name, flags, mode);
	elf_push_handle(S, file);
	return 1;
}

ELF_FUNCTION(l_sys_close_file) {
	ASSERT(elf_get_num_args(S) == 1);
	elf_Handle file = elf_get_sysarg(S,0);
	if (file) {
		sys_close_file(file);
	}
	return 0;
}

ELF_FUNCTION(l_sys_get_file_size) {
	elf_Handle file = elf_get_sysarg(S,0);
	elf_push_int(S, sys_size_file(file));
	return 1;
}

ELF_FUNCTION(l_sys_get_file_cursor) {
	elf_Handle file = elf_get_sysarg(S,0);
	elf_push_int(S, sys_get_file_cursor(file));
	return 1;
}

ELF_FUNCTION(l_sys_read_file) {
	// for this sort of stuff, we could just reposition
	// the stack pointer... ?
	// todo: also make this be public lib or something
	// or make read file take a stack address
	* S->stack_ptr ++ = elf_get_arg(S, 0);
	elf_read_file(S, -1);
	return 1;
}

ELF_FUNCTION(l_sys_write_file) {
	elf_Handle file = elf_get_sysarg(S,0);
	elf_String *str = elf_get_string_arg(S, 1);
	sys_write_file(file, str->text, str->length);
	return 0;
}

ELF_FUNCTION(l_sys_write_file_to_file) {
	elf_Handle dst = elf_get_sysarg(S, 0);
	elf_Handle src = elf_get_sysarg(S, 1);
	int size = sys_size_file(src);
	char *heapbuf = malloc(size);
	sys_read_file(src, heapbuf, size);
	sys_write_file(dst, heapbuf, size);
	free(heapbuf);
	return 1;
}


ELF_FUNCTION(l_sys_change_work_dir) {
	int noerr = sys_set_work_dir(elf_get_text_arg(S,0));
	elf_push_int(S, noerr);
	return 1;
}


ELF_FUNCTION(l_sys_get_work_dir) {
	char buf[256];
	sys_get_work_dir(buf, sizeof(buf));
	elf_push_string(S, buf);
	return 1;
}


// process
ELF_FUNCTION(l_sys_create_process) {
	char *args = elf_get_text_arg(S, 0);
	elf_Handle process = sys_create_process(0, args);
	elf_push_handle(S, process);
	return 1;
}

ELF_FUNCTION(l_sys_exit_this_process) {
	sys_exit_this_process(elf_get_intarg(S, 0));
	return 0;
}

ELF_FUNCTION(l_sys_get_this_process_id) {
	int id = sys_get_this_process_id();
	elf_push_int(S, id);
	return 1;
}


static const elf_Binding l_sys[] = {
	{"load_dll",                  l_sys_load_dll                  },
	{"get_dll_fn",                l_sys_get_dll_fn                },

	{"get_file_tree",             l_sys_get_file_tree             },
	{"get_path_list",             l_sys_get_path_list             },

	{"open_temp_file",            l_sys_open_temp_file            },
	{"open_file",                 l_sys_open_file                 },
	{"close_file",                l_sys_close_file                },
	{"get_file_size",             l_sys_get_file_size             },
	{"read_file",                 l_sys_read_file                 },
	{"get_file_cursor",           l_sys_get_file_cursor           },
	{"write_file",                l_sys_write_file                },
	{"write_file_to_file",        l_sys_write_file_to_file        },
	{"change_work_dir",           l_sys_change_work_dir           },
	{"get_work_dir",              l_sys_get_work_dir              },

	{"get_file_times",            l_sys_get_file_times            },
	{"file_time_to_system_time",  l_sys_file_time_to_system_time  },
	{"sleep",                     l_sys_sleep                     },
	{"get_file_name_from_path",   l_sys_get_file_name_from_path   },
	{"create_directory",          l_sys_create_directory          },
	{"delete_file",               l_sys_delete_file               },

	{"create_process",            l_sys_create_process            },
	{"exit",                      l_sys_exit_this_process         },
	{"get_process_id",            l_sys_get_this_process_id       },


	{"get_performance_counter",              l_sys_get_performance_counter                  },
	{"get_performance_counter_frequency",    l_sys_get_performance_counter_frequency        },
	{"get_performance_counter_elapsed_s",    l_sys_get_performance_counter_elapsed_s        },
	{"get_performance_counter_elapsed_ms",   l_sys_get_performance_counter_elapsed_ms       },
};