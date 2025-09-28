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
// PATHS
//






ELF_FUNCTION(l_sys_get_parent_path) {
	Str p = loadstr(S, 1);
	int n = 1;
	if (nargs >= 3) {
		n = loadint(S, 2);
	}

	const char *s = strt(p);
	const char *e = strt(p) + strl(p);

	while (n -- > 0) {
		do e --; while(e > s && *e != '\\' && *e != '/');
	}

	pushtext2(S, s, e - s);

	return 1;
}


static char *slice_path(char *p, int *l, int n) {
	char *e = p + *l;

	if (n < 0) {
		while (n ++) {
			e --;
			while (e > p && e[-1] != '\\' && e[-1] != '/') e --;
		}
		*l = *l - (e - p);
		return e;
	}
	else {
		char *s = p;
		while (n --) {
			do s ++; while (s < e && *s != '\\' && *s != '/');
		}
		*l = s - p;
		return p;
	}
}

//
//
//
//
//
//
//
//
//
//
//
//
//
//
//
//

ELF_FUNCTION(l_sys_slice_path) {
	Str p = loadstr(S, 1);
	int n = 1;
	if (nargs > 2) {
		n = loadint(S, 2);
	}

	int l = strl(p);
	char *s = slice_path((char *) strt(p), &l, n);

	if (nargs > 3) {
		n = loadint(S, 3);
		s = slice_path(s, &l, n);
	}

	pushtext2(S, s, l);
	return 1;
}

//
//
//
//
//
//
//
//
//
//
//
//
//
//
//
//



//
// some      -> some
// some.name -> some
//           ->
//
// .../some.name -> some
// .../some      -> some
// .../          ->
//
//

ELF_FUNCTION(l_sys_get_file_name) {
	Str p = loadstr(S, 1);
	const char *s = strt(p);
	const char *e = strt(p) + strl(p);
	const char *d = e;

	while (e > s && *e != '\\' && *e != '/' && *e != '.') e --;
	if (*e == '.') d = e;
	while (e > s && e[-1] != '\\' && e[-1] != '/') e --;

	pushtext2(S, e, d - e);
	return 1;
}

//
//
//
//
//
//
//
//
//
//
//
//
//
//
//
//

ELF_FUNCTION(l_sys_get_file_extension) {
	Str p = loadstr(S, 1);
	const char *s = strt(p);
	const char *e = strt(p) + strl(p);
	while (e > s && e[-1] != '\\' && e[-1] != '/' && e[-1] != '.') e --;
	pushtext2(S, e, strl(p) - (e - strt(p)));
	return 1;
}

//
//
//
//
//
//
//
//
//
//
//
//
//
//
//
//

ELF_FUNCTION(l_sys_create_directory) {
	const char *path = loadtext(S, 1);
	int noerr = sys_make_dir(path);
	elf_pushint(S, noerr);
	return 1;
}

//
//
//
//
//
//
//
//
//
//
//
//
//
//
//
//

ELF_FUNCTION(l_sys_get_file_times) {
	Handle file = loadsys(S, 1);

	FILE_TIMES times;
	sys_time_file(file, &times);

	// todo: set fields directly
	Tab tab = pushnewtable(S);
	elf_pushtext(S, "created");  elf_pushint(S, times.create.time); elf_setfield(S);
	elf_pushtext(S, "access");   elf_pushint(S, times.access.time); elf_setfield(S);
	elf_pushtext(S, "write");    elf_pushint(S, times.write.time);  elf_setfield(S);
	return 1;
}

//
//
//
//
//
//
//
//
//
//
//
//
//
//
//
//

ELF_FUNCTION(l_sys_file_time_to_system_time) {

	Time time = loadint(S, 1);
	FILE_TIME filetime = { .time = time };

	SYSTEM_TIME systemtime;
	sys_file_time_to_system_time(&filetime, &systemtime);

	pushnewtable(S);

	pushtext(S, "year");
	pushint(S, systemtime.year);
	elf_setfield(S);
	pushtext(S, "month");
	pushint(S, systemtime.month);
	elf_setfield(S);
	pushtext(S, "dayofweek");
	pushint(S, systemtime.dayofweek);
	elf_setfield(S);
	pushtext(S, "day");
	pushint(S, systemtime.day);
	elf_setfield(S);
	pushtext(S, "hour");
	pushint(S, systemtime.hour);
	elf_setfield(S);
	pushtext(S, "minute");
	pushint(S, systemtime.minute);
	elf_setfield(S);
	pushtext(S, "second");
	pushint(S, systemtime.second);
	elf_setfield(S);
	pushtext(S, "milliseconds");
	pushint(S, systemtime.milliseconds);
	elf_setfield(S);
	return 1;
}
//
//
//
//
//
//
//
//
//
//
//
//
//
//
//
//

//
// name: the name of the dynamic library in
// the file system
//
ELF_FUNCTION(l_sys_load_dll) {
	const char *name = loadtext(S, 1);

	Sys dll = sys_load_dll(name);

	if (dll != 0) pushsys(S, dll);
	else          pushnil(S);
	return 1;
}

//
//
//
//
//
//
//
//
//
//
//
//
//
//
//
//

ELF_FUNCTION(l_sys_get_dll_fn) {
	Sys dll = loadsys(S, 1);
	const char *name = loadtext(S, 2);

	Fun fun = (Fun) sys_get_dll_fn(dll, name);
	if (fun != 0) pushfun(S,fun);
	else          pushnil(S);
	return 1;
}

//
//
//
//
//
//
//
//
//
//
//
//
//
//
//
//

// NOTE: requires a table on the stack!
// TODO: use utility function to ensure this!
static void pathlist(elf_State *inter, FILE_VISITOR *visitor, int recurse) {

	Sys dir = sys_find_first_file(visitor);
	if (!dir) goto esc;

	for (;;)
	{
		if (visitor->type == FILE_TYPE_SYMLINK) {
			goto skip;
		}

		if (visitor->pb.type != PATH_NAME) {
			goto skip;
		}

		elf_pushtext(inter, visitor->pb.path);
		elf_arrayadd(inter);

		if (visitor->type == FILE_TYPE_FOLDER) {
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

//
//
//
//
//
//
//
//
//
//
//
//
//
//
//
//

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

//
//
//
//
//
//
//
//
//
//
//
//
//
//
//
//


//
// returns all child paths including folders
// todo: add filters!
//
ELF_FUNCTION(l_sys_get_path_list) {

	const char *path = loadtext(S, 1);

	int recurse = 0;
	if (nargs >= 3) {
		recurse = loadint(S, 2);
	}

	elf_pushtab(S);

	FILE_VISITOR *visi = calloc(1, sizeof(*visi));

	pb_push(&visi->pb, path);
	pathlist(S, visi, recurse);

	// todo:
	free(visi->pb.sb.buf);
	free(visi);
	return 1;
}

//
//
//
//
//
//
//
//
//
//
//
//
//	todo: remove this!
//
//
//

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

//
//
//
//
//
//
//
//
//
//
//
//
//
//
//
//

ELF_FUNCTION(l_sys_delete_file) {
	char const *path = loadtext(S, 1);
	int ok = sys_delete_file(path);
	elf_pushint(S, ok);
	return 1;
}

//
//
//
//
//
//
//
//
//
//
//
//
//
//
//
//

ELF_FUNCTION(l_sys_open_file) {
	const char *name = loadtext(S, 1);
	const char *text = loadtext(S, 2);

	int flags;
	for (flags = 0; *text; text ++) {
		if      (*text == 'r') flags |= SYS_OPEN_READ;
		else if (*text == 'w') flags |= SYS_OPEN_WRITE;
		else if (*text == 'b') flags |= 0;
		else reporterror(S, NO_BYTE, "unrecognized flag");
	}

	int mode = SYS_OPEN_EXISTING;
	if (flags & SYS_OPEN_WRITE) {
		mode = SYS_CREATE_ALWAYS;
	}

	Sys file = sys_open_file(name, flags, mode);

	if (ELF_HISINVALID(file)) {
		pushnil(S);
	}
	else {
		pushsys(S, file);
	}
	return 1;
}

//
//
//
//
//
//
//
//
//
//
//
//
//
//
//
//

ELF_FUNCTION(l_sys_close_file) {
	Handle file = loadsys(S, 1);
	if (file) {
		sys_close_file(file);
	}
	return 0;
}


//
//
//
//
//
//
//
//
//
//
//
//
//
//
//
//



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
	Str contents = new_emptystr(S, size);
	to_str(S->stack_ptr, contents);
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

	if (is_string_type(loadtype(S, 1))) {

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

		reporterrorf(S, -1
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

	{"slice_path",                l_sys_slice_path                },
	{"get_file_name",             l_sys_get_file_name             },
	{"get_file_extension",        l_sys_get_file_extension        },
	{"get_parent_path",           l_sys_get_parent_path           },

	{"create_directory",          l_sys_create_directory          },
	{"delete_file",               l_sys_delete_file               },

	{"create_process",            l_sys_create_process            },
	{"get_process_id",            l_sys_get_this_process_id       },
	{"exit",                      l_sys_exit_this_process         },


	{"get_perf_counter",      l_sys_get_performance_counter             },
	{"get_perf_frequency",    l_sys_get_performance_counter_frequency   },
	{"get_perf_elapsed_s",    l_sys_get_performance_counter_elapsed_s   },
	{"get_perf_elapsed_ms",   l_sys_get_performance_counter_elapsed_ms  },
};