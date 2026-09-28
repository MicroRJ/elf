//
// Optional file-backed serialization operations.
//

#include "elf_os_services.h"

ELF_FUNCTION(l_serialization_load_json_file)
{
	lib_check_arg_count(S, "serialization.load_json_file", nargs, 1, 1);
	const char *name = lib_load_cstr(S, 1);
	elf_OS_FileInfo info;
	if (!elf_os_get_file_info(name, &info) || info.is_directory || info.size > INT_MAX) {
		elf_push_nil(S);
		return 1;
	}
	elf_OS_File file = elf_os_open_file_read(name);
	if (!elf_os_file_is_valid(file)) {
		elf_push_nil(S);
		return 1;
	}

	char *data = malloc((size_t)info.size + 1);
	if (!data) {
		elf_os_close_file(file);
		elf_push_nil(S);
		return 1;
	}
	elf_u64 size = 0;
	elf_b32 success = elf_os_read_file(file, data, info.size, &size);
	elf_os_close_file(file);
	if (!success || size != info.size) {
		free(data);
		elf_push_nil(S);
		return 1;
	}

	data[size] = 0;
	elf_StrSlice source = {data, size};
	if (elf_push_json(S, name, source) != ELF_ERROR_NONE) elf_push_nil(S);
	free(data);
	return 1;
}

static const Battery_Binding l_serialization[] = {
	{"load_json_file", l_serialization_load_json_file},
};

static void elf_lib_serialization(elf_State *state)
{
	new_binding_table(state, l_serialization, battery_array_count(sizeof(l_serialization), sizeof(l_serialization[0])));
}
