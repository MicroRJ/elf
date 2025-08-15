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
	elf_pushint(S, sys_get_performance_counter());
	return 1;
}

ELF_FUNCTION(l_sys_get_performance_counter_frequency) {
	elf_pushint(S, sys_get_performance_counter_frequency());
	return 1;
}

ELF_FUNCTION(l_sys_get_performance_counter_elapsed_s) {
	ASSERT((nargs - 1) == 1);
	elf_i64 time = elf_toint(S, args + 1 + 0);
	elf_pushnum(S, get_performance_counter_elapsed_s(time));
	return 1;
}

ELF_FUNCTION(l_sys_get_performance_counter_elapsed_ms) {
	ASSERT((nargs - 1) == 1);
	elf_i64 time = elf_toint(S, args + 1 + 0);
	elf_pushnum(S, get_performance_counter_elapsed_s(time) * 1000);
	return 1;
}

ELF_FUNCTION(l_sys_sleep) {
	ASSERT((nargs - 1) >= 1);
	sys_sleep(elf_toint(S, args + 1 + 0));
	return 0;
}

//
// files
//

ELF_FUNCTION(l_sys_get_file_name_from_path) {
	const char *path = elf_tostr(S, args + 1);
	const char *name = get_name_from_file_path(path);
	elf_pushstr(S, name);
	return 1;
}


ELF_FUNCTION(l_sys_create_directory) {
	const char *path = elf_tostr(S, args + 1);
	int noerr = sys_create_directory(path);
	elf_pushint(S, noerr);
	return 1;
}


ELF_FUNCTION(l_sys_get_file_times) {
	elf_Handle file = f_checkhand(S, 0);

	FILE_TIMES times;
	sys_time_file(file, &times);

	elf_pushtab(S);
	elf_pushstr(S, "created");  elf_pushint(S, times.create.time); elf_setfield(S);
	elf_pushstr(S, "access");   elf_pushint(S, times.access.time); elf_setfield(S);
	elf_pushstr(S, "write");    elf_pushint(S, times.write.time);  elf_setfield(S);
	return 1;
}

ELF_FUNCTION(l_sys_file_time_to_system_time) {

	elf_Integer time = elf_toint(S, args + 1 + 0);
	FILE_TIME filetime = { .time = time };

	SYSTEM_TIME systemtime;
	sys_file_time_to_system_time(&filetime, &systemtime);

	elf_pushtab(S);

	elf_pushstr(S, "year");         elf_pushint(S, systemtime.year);           elf_setfield(S);
	elf_pushstr(S, "month");        elf_pushint(S, systemtime.month);          elf_setfield(S);
	elf_pushstr(S, "dayofweek");    elf_pushint(S, systemtime.dayofweek);      elf_setfield(S);
	elf_pushstr(S, "day");          elf_pushint(S, systemtime.day);            elf_setfield(S);
	elf_pushstr(S, "hour");         elf_pushint(S, systemtime.hour);           elf_setfield(S);
	elf_pushstr(S, "minute");       elf_pushint(S, systemtime.minute);         elf_setfield(S);
	elf_pushstr(S, "second");       elf_pushint(S, systemtime.second);         elf_setfield(S);
	elf_pushstr(S, "milliseconds"); elf_pushint(S, systemtime.milliseconds);   elf_setfield(S);
	return 1;
}

//
// name: the name of the dynamic library in
// the file system
//
ELF_FUNCTION(l_sys_load_dll) {
	char *name = f_checktext(S, 0);

	elf_Handle lib = sys_load_dll(name);
	if (lib != 0) elf_pushsys(S, lib);
	else          elf_pushnil(S);
	return 1;
}

ELF_FUNCTION(l_sys_get_dll_fn) {
	elf_Handle lib = elf_tosys(S, args + 1);
	const char *name = elf_tostr(S, args + 2);

	elf_Function fn = (elf_Function) sys_get_dll_fn(lib, name);
	if (fn != 0) elf_pushfun(S,fn);
	else         elf_pushnil(S);
	return 1;
}

#if 0


static void filetreecompressed(FILE_VISITOR *visitor, File_Node *node, int recurse) {
	if (sys_find_first_file(visitor)) do {
		if (visitor->type == FILE_TYPE_SYMLINK) continue;
		if (is_file_name_empty(visitor->name)) continue;

		int index = darr_grow(visitor->nodes, 1);
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
	} while (sys_find_next_file(visitor));
}


static elf_stkid filetree(elf_State *inter, char *path, int recurse, int *size) {

	elf_stkid resstk = elf_pushtab(inter);

	const char *type2s[] = {
		[FILE_TYPE_FILE] = "file",
		[FILE_TYPE_FOLDER] = "folder",
		[FILE_TYPE_SYMLINK] = "symlink",
	};

	elf_pushstr(inter, "path");
	elf_pushstr(inter, path);
	elf_setfield(inter);

	elf_pushstr(inter, "type");
	elf_pushstr(inter, type2s[visitor.type]);
	elf_setfield(inter);


	elf_pushtab(inter);


	// todo: speed! we can pre-push all these strings and reference
	// them by stack address instead!
	FILE_VISITOR visitor;
	if (sys_find_first_file(&visitor, path)) do {
		if (visitor.type == FILE_TYPE_SYMLINK) continue;
		if (is_file_name_empty(visitor.name)) continue;

		// todo: can we have our cake and eat it too please?
		char *childpath = malloc(1024);
		stbsp_snprintf(childpath, 1024, "%s\\%s", path, visitor.name);

		elf_pushtab(inter);
		{

			if (visitor.type == FILE_TYPE_FOLDER) {
				if (recurse > 0) {
					elf_pushstr(inter, "children");
					int childsize = 0;
					elf_stkid child = filetree(inter, childpath, recurse - 1, &childsize);

					// todo:
					elf_pushstr(inter, "parent");
					* inter->stack_ptr ++ = inter->stack[child];
					elf_setfield(inter);

					elf_setfield(inter);

					*size += childsize;
				}

				elf_pushstr(inter, "size");
				elf_pushint(inter, visitor.size);
				elf_setfield(inter);
			} else {
				elf_pushstr(inter, "size");
				elf_pushint(inter, visitor.size);
				elf_setfield(inter);

				*size += visitor.size;
			}

			free(childpath);
		}
		elf_arrayadd(inter);

	} while (sys_find_next_file(&visitor));

	return resstk;
}
#endif


// NOTE: requires a table on the stack!
// TODO: use utility function to ensure this!
static void pathlist(elf_State *inter, FILE_VISITOR *visitor, int recurse) {

	elf_Handle dir = sys_find_first_file(visitor);
	if (!dir) goto esc;

	for (;;)
	{

		if (visitor->type == FILE_TYPE_SYMLINK) {
			goto skip;
		}

		if (visitor->pb.type != PATH_NAME) {
			goto skip;
		}
		//	char *name = get_name_from_file_path(visitor->pb.path);
		//	if (is_file_name_empty(name)) goto nop;

		if (visitor->type == FILE_TYPE_FILE) {

			elf_pushstr(inter, visitor->pb.path);
			elf_arrayadd(inter);

		}
		else if (visitor->type == FILE_TYPE_FOLDER) {

			if (recurse > 0) {
				pathlist(inter, visitor, recurse - 1);
			}
		}

		skip:
		if (!sys_find_next_file(dir, visitor)) {
			goto esc;
		}
	}

	esc: ;
}


ELF_FUNCTION(l_sys_get_file_tree) {

	const char *path = elf_tostr(S, args + 1);

	int recurse = 0;
	if (nargs >= 3) {
		recurse = elf_toint(S, args + 2);
	}

	// filetree(S, path, recurse);

	elf_pushnil(S);
	return 1;
}

ELF_FUNCTION(l_sys_get_path_list) {

	const char *path = elf_tostr(S, args + 1);

	int recurse = 0;
	if (nargs >= 3) {
		recurse = elf_toint(S, args + 2);
	}

	elf_pushtab(S);

	FILE_VISITOR *visi = calloc(1, sizeof(*visi));

	pushpath(&visi->pb, path);
	pathlist(S, visi, recurse);

	// todo:
	free(visi->pb.sb.buf);
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
	elf_pushsys(S,(elf_Handle)file);
	return 1;
}

ELF_FUNCTION(l_sys_delete_file) {
	elf_pushint(S, sys_delete_file(f_checktext(S, 0)));
	return 1;
}

ELF_FUNCTION(l_sys_open_file) {
	ASSERT((nargs - 1) == 2);

	char *name = f_checktext(S,0);
	char *text_flags = f_checktext(S,1);

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
	if (ELF_HISINVALID(file)) {
		pushnil(S);
	} else {
		pushsys(S, file);
	}
	return 1;
}

ELF_FUNCTION(l_sys_close_file) {
	ASSERT((nargs - 1) == 1);
	elf_Handle file = f_checkhand(S,0);
	if (file) {
		sys_close_file(file);
	}
	return 0;
}

ELF_FUNCTION(l_sys_get_file_size) {
	elf_Handle file = f_checkhand(S,0);
	elf_pushint(S, sys_size_file(file));
	return 1;
}

ELF_FUNCTION(l_sys_get_file_cursor) {
	elf_Handle file = f_checkhand(S,0);
	elf_pushint(S, sys_get_file_cursor(file));
	return 1;
}


elf_pubapi
bool elf_readfile(elf_State *inter, int stk, int size);


// todo: should take a buffer
ELF_FUNCTION(l_sys_read_console) {
	int zbuf = loadint(S, 1);
	char *buf = calloc(1, zbuf + 1);
	int ret = sys_read_console(SYS_STD_INPUT, buf, zbuf);
	pushstring(S, buf);
	return 1;
}


//
// @doc sys.read_file(name or handle, size) -> contents
//
ELF_FUNCTION(l_sys_read_file) {
	int size = -1;
	if (nargs >= 3) {
		size = elf_toint(S, 2);
	}
	elf_readfile(S, args + 1, size);
	return 1;
}


ELF_FUNCTION(l_sys_write_file) {
	elf_Handle file = f_checkhand(S,0);
	elf_String *str = f_checkstr(S, 1);
	sys_write_file(file, str->text, str->length);
	return 0;
}

ELF_FUNCTION(l_sys_write_file_to_file) {
	elf_Handle dst = f_checkhand(S, 0);
	elf_Handle src = f_checkhand(S, 1);
	int size = sys_size_file(src);
	char *heapbuf = malloc(size);
	sys_read_file(src, heapbuf, size);
	sys_write_file(dst, heapbuf, size);
	free(heapbuf);
	return 1;
}


ELF_FUNCTION(l_sys_change_work_dir) {
	int noerr = sys_set_work_dir(f_checktext(S,0));
	elf_pushint(S, noerr);
	return 1;
}


ELF_FUNCTION(l_sys_get_work_dir) {
	char buf[256];
	sys_get_work_dir(buf, sizeof(buf));
	elf_pushstr(S, buf);
	return 1;
}


// process
ELF_FUNCTION(l_sys_create_process) {
	char *textargs = f_checktext(S, 0);
	elf_Handle process = sys_create_process(0, textargs);
	elf_pushsys(S, process);
	return 1;
}


ELF_FUNCTION(l_sys_exit_this_process) {
	sys_exit_this_process(elf_toint(S, args + 1 + 0));
	return 0;
}

ELF_FUNCTION(l_sys_get_this_process_id) {
	int id = sys_get_this_process_id();
	elf_pushint(S, id);
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
	{"read_console",              l_sys_read_console                 },
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