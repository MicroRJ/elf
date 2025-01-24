/*
** See Copyright Notice In elf.h
** lib_core.h
*/

// _(pause_collector)
// _(get_allocated_objects)
// _(get_allocated_memory)
// _(get_collector_threshold)
// _(mark_object)
// _(collect)

// todo: there are functions here that should
// be under elf.sys... instead... idk?
#define LIBDEF(_) \
_(float2)\
_(unload)\
_(tagof)\
_(get_global)\
_(get_object_color)\
_(set_object_trap)\
_(get_object_address)\
_(merge_tables)\
_(get_meta)\
_(set_meta)\
_(abort)\
_(exit)\
_(flags)\
_(debugger)\
_(log)\
_(err_log)\
_(include)\
_(const_expr)\
_(load_file)\
_(load_json)\
_(load_expr)\
_(pf)\
_(ntoi)\
_(iton)\
_(load_dll)\
_(get_dll_fn)\
_(open_file)\
_(close_file)\
_(get_file_size)\
_(read_file)\
_(write_file)\
_(write_file_to_file)\
_(open_temp_file)\
_(change_work_dir)\
_(get_work_dir)\
_(list_folder)\
/* end */

#define LIBNAME(NAME) static int core_lib_##NAME(elf_State *R);
LIBDEF(LIBNAME)
#undef LIBNAME

// todo: sort these in order of relevance
static elf_CBinding lib_core[] = {
#define LIBNAME(NAME) {#NAME,core_lib_##NAME},
	LIBDEF(LIBNAME)
#undef LIBNAME
	// {"shell", lib_core_shell},
	// {"exec", core_lib_exec},
	// {"get_disk_info", core_lib_get_disk_info},
	// {"list_volumes", core_lib_list_volumes},
	// {"enumerate_folder", core_lib_enumerate_folder},
};

#undef LIBDEF
