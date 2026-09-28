#include "elf.h"

int main(void)
{
	int result_code = 0;
	elf_State *state = elf_create_state();
	elf_get_global(state, "elf");
	if (!elf_get_field(state, -1, "fs") || !elf_is_nil(state, -1)) {
		result_code = 1;
		goto done;
	}
	elf_pop(state, 2);

	char source_text[] = "ret 42";
	elf_StrSlice source = {source_text, sizeof(source_text) - 1};
	if (elf_push_code_source(state, "core_tests", source, 0) != ELF_ERROR_NONE) {
		result_code = 2;
		goto done;
	}
	elf_push_nil(state);
	elf_call(state, 1, 1);
	elf_Int result = 0;
	if (!elf_to_int(state, -1, &result) || result != 42) {
		result_code = 3;
	}

done:
	elf_destroy_state(state);
	return result_code;
}
