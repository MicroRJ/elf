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
	Time time = loadint(S, 1);
	elf_pushnum(S, get_performance_counter_elapsed_s(time));
	return 1;
}



ELF_FUNCTION(l_sys_get_performance_counter_elapsed_ms) {
	Time time = loadint(S, 1);
	elf_pushnum(S, get_performance_counter_elapsed_s(time) * 1000);
	return 1;
}



ELF_FUNCTION(l_sys_sleep) {
	sys_sleep(loadint(S, 1));
	return 0;
}



//
// files
//

ELF_FUNCTION(l_sys_get_file_name_from_path) {
	const char *path = elf_loadtext(S, 1);
	const char *name = get_name_from_file_path(path);
	elf_pushtext(S, name);
	return 1;
}


ELF_FUNCTION(l_sys_create_directory) {
	const char *path = elf_loadtext(S, 1);
	int noerr = sys_make_dir(path);
	elf_pushint(S, noerr);
	return 1;
}


ELF_FUNCTION(l_sys_get_file_times) {
	Handle file = loadsys(S, 1);

	FILE_TIMES times;
	sys_time_file(file, &times);

	// todo: set fields directly
	Tab tab = pushtable(S);
	elf_pushtext(S, "created");  elf_pushint(S, times.create.time); elf_setfield(S);
	elf_pushtext(S, "access");   elf_pushint(S, times.access.time); elf_setfield(S);
	elf_pushtext(S, "write");    elf_pushint(S, times.write.time);  elf_setfield(S);
	return 1;
}

ELF_FUNCTION(l_sys_file_time_to_system_time) {

	Time time = loadint(S, 1);
	FILE_TIME filetime = { .time = time };

	SYSTEM_TIME systemtime;
	sys_file_time_to_system_time(&filetime, &systemtime);

	pushtable(S);

	pushtext(S, "year");         pushint(S, systemtime.year);           elf_setfield(S);
	pushtext(S, "month");        pushint(S, systemtime.month);          elf_setfield(S);
	pushtext(S, "dayofweek");    pushint(S, systemtime.dayofweek);      elf_setfield(S);
	pushtext(S, "day");          pushint(S, systemtime.day);            elf_setfield(S);
	pushtext(S, "hour");         pushint(S, systemtime.hour);           elf_setfield(S);
	pushtext(S, "minute");       pushint(S, systemtime.minute);         elf_setfield(S);
	pushtext(S, "second");       pushint(S, systemtime.second);         elf_setfield(S);
	pushtext(S, "milliseconds"); pushint(S, systemtime.milliseconds);   elf_setfield(S);
	return 1;
}



//
// name: the name of the dynamic library in
// the file system
//
ELF_FUNCTION(l_sys_load_dll) {
	const char *name = loadtext(S, 1);

	Handle dll = sys_load_dll(name);

	if (dll != 0) pushsys(S, dll);
	else          pushnil(S);
	return 1;
}



ELF_FUNCTION(l_sys_get_dll_fn) {
	Handle dll = loadsys(S, 1);
	const char *name = loadtext(S, 2);

	elf_Function fun = (elf_Function) sys_get_dll_fn(dll, name);
	if (fun != 0) pushfun(S,fun);
	else          pushnil(S);
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

	elf_pushtext(inter, "path");
	elf_pushtext(inter, path);
	elf_setfield(inter);

	elf_pushtext(inter, "type");
	elf_pushtext(inter, type2s[visitor.type]);
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
					elf_pushtext(inter, "children");
					int childsize = 0;
					elf_stkid child = filetree(inter, childpath, recurse - 1, &childsize);

					// todo:
					elf_pushtext(inter, "parent");
					* inter->stack_ptr ++ = inter->stack[child];
					elf_setfield(inter);

					elf_setfield(inter);

					*size += childsize;
				}

				elf_pushtext(inter, "size");
				elf_pushint(inter, visitor.size);
				elf_setfield(inter);
			} else {
				elf_pushtext(inter, "size");
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

			elf_pushtext(inter, visitor->pb.path);
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

	const char *path = loadtext(S, 1);

	int recurse = 0;
	if (nargs >= 3) {
		recurse = loadint(S, 2);
	}

	// filetree(S, path, recurse);

	elf_pushnil(S);
	return 1;
}

ELF_FUNCTION(l_sys_get_path_list) {

	const char *path = loadtext(S, 1);

	int recurse = 0;
	if (nargs >= 3) {
		recurse = loadint(S, 2);
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
	elf_pushint(S, sys_delete_file(elf_loadtext(S, 0)));
	return 1;
}

ELF_FUNCTION(l_sys_open_file) {
	const char *name = loadtext(S, 1);
	const char *text_flags = loadtext(S, 2);

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
	Handle file = loadsys(S, 1);
	if (file) {
		sys_close_file(file);
	}
	return 0;
}



ELF_FUNCTION(l_sys_get_file_size) {
	Handle file = loadsys(S, 1);
	pushint(S, sys_size_file(file));
	return 1;
}



ELF_FUNCTION(l_sys_move_file_cursor) {
	Handle file = loadsys(S, 1);
	Int relativeto = loadint(S, 2);
	Int distance = loadint(S, 3);
	pushint(S, sys_move_file_cursor(file, relativeto, distance));
	return 1;
}



// todo: should take a buffer
ELF_FUNCTION(l_sys_read_console) {
	int zbuf = loadint(S, 1);
	char *buf = calloc(1, zbuf + 1);
	int ret = sys_read_console(SYS_STD_INPUT, buf, zbuf);
	pushtext(S, buf);
	return 1;
}



static bool readfilesys(elf_State *S, Handle file, int size) {
	if (!file) {
		elf_lerror("invalid file handle");
		pushnil(S);
		return false;
	}

	if (size == -1) {
		size = sys_size_file(file);
	}

	// todo: we need a dedicated object for this!
	Str contents = elf_alloc_string2(S, size);
	vsetstr(S->stack_ptr, contents);
	pushstacksafe(S);

	sys_read_file(file, contents->text, size);

	return true;
}



//
// @doc sys.read_file(name or handle, size) -> contents
//
ELF_FUNCTION(l_sys_read_file) {
	int size = -1;
	if (nargs >= 3) {
		size = loadint(S, 2);
	}

	int noerr = 0;

	if (tisstr(loadtype(S, 1))) {

		const char *name = loadtext(S, 1);

		Handle file = sys_open_file(name, SYS_OPEN_READ, SYS_OPEN_EXISTING);

		noerr = readfilesys(S, file, size);

		sys_close_file(file);
	}
	else if (tissys(loadtype(S, 1))) {

		Handle file = loadsys(S, 1);

		noerr = readfilesys(S, file, size);
	}
	else {

		elf_errorf(S, -1
		, "'%s': 'readfile' expected handle or file name", tag2s[loadtype(S, 1)]);

		pushnil(S);
	}
	return 1;
}



ELF_FUNCTION(l_sys_write_file) {
	Handle file = loadsys(S, 1);
	Str str = loadstr(S, 2);
	sys_write_file(file, str->text, str->length);
	return 0;
}



ELF_FUNCTION(l_sys_write_file_to_file) {
	Handle dst = loadsys(S, 1);
	Handle src = loadsys(S, 2);

	int size = sys_size_file(src);
	char *heapbuf = malloc(size);
	sys_read_file(src, heapbuf, size);
	sys_write_file(dst, heapbuf, size);
	free(heapbuf);
	return 1;
}



ELF_FUNCTION(l_sys_change_work_dir) {
	int noerr = sys_set_work_dir(loadtext(S, 1));
	pushint(S, noerr);
	return 1;
}



ELF_FUNCTION(l_sys_get_work_dir) {
	char buf[256];
	sys_get_work_dir(buf, sizeof(buf));
	pushtext(S, buf);
	return 1;
}


// process
ELF_FUNCTION(l_sys_create_process) {
	const char *text = loadtext(S, 1);
	Handle process = sys_create_process(0, text);
	pushsys(S, process);
	return 1;
}


ELF_FUNCTION(l_sys_exit_this_process) {
	sys_exit_this_process(loadint(S, 1));
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
	{"read_console",              l_sys_read_console              },
	{"move_file_cursor",          l_sys_move_file_cursor          },
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