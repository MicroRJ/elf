#include <elf.h>
#include <elf_dll.h>

_Static_assert(sizeof(elf_Size) == sizeof(size_t), "elf_Size must match size_t");
_Static_assert(ELF_API_VERSION >= 2, "public API must include array support");
_Static_assert(ELF_DLL_API_VERSION == 1, "DLL API must begin at version 1");

static const elf_Api dll_api = {
	.type = elf_type,
	.to_int = elf_to_int,
	.to_num = elf_to_num,
	.to_str = elf_to_str,
	.push_nil = elf_push_nil,
	.push_int = elf_push_int,
	.push_num = elf_push_num,
	.push_str = elf_push_str,
	.push_error = elf_push_error,
	.push_fun = elf_push_fun,
	.new_table = elf_new_table,
	.set_field = elf_set_field_slice,
};

static elf_DLL_Open *dll_open;

void public_header_compile_test(void)
{
	dll_open = 0;
	(void)dll_api;
	(void)dll_open;
}
