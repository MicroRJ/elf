#ifndef ELF_DLL_H
#define ELF_DLL_H

#include <elf.h>

#define ELF_DLL_API_VERSION 1
#define ELF_DLL_ENTRY_NAME "elf_dll_open_v1"

#if defined(_WIN32)
#define ELF_DLL_EXPORT __declspec(dllexport)
#else
#define ELF_DLL_EXPORT
#endif

typedef struct elf_Api
{
	elf_ValueType (*type)(elf_State *state, elf_Index index);
	elf_Bool (*to_int)(elf_State *state, elf_Index index, elf_Int *value);
	elf_Bool (*to_num)(elf_State *state, elf_Index index, elf_Num *value);
	elf_Bool (*to_str)(elf_State *state, elf_Index index, elf_StrSlice *value);

	void (*push_nil)(elf_State *state);
	void (*push_int)(elf_State *state, elf_Int value);
	void (*push_num)(elf_State *state, elf_Num value);
	void (*push_str)(elf_State *state, const char *data, elf_Size size);
	void (*push_error)(elf_State *state, const char *message);
	void (*push_fun)(elf_State *state, elf_Function function);

	void (*new_table)(elf_State *state);
	elf_ErrorCode (*set_field)(elf_State *state, elf_Index index, elf_StrSlice field);
}
elf_Api;

typedef elf_ErrorCode elf_DLL_Open(const elf_Api *api, elf_State *state);

#define ELF_DLL_ENTRY ELF_DLL_EXPORT elf_ErrorCode elf_dll_open_v1(const elf_Api *api, elf_State *state)

#endif
