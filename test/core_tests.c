#include "elf.h"

int main(void)
{
	int result_code = 0;
	elf_State *state = elf_create_state();
	elf_get_global(state, "elf");
	if (!elf_get_field(state, -1, "fs") || elf_type(state, -1) != ELF_VALUE_TYPE_TABLE) {
		result_code = 1;
		goto done;
	}
	elf_pop(state, 2);

done:
	elf_destroy_state(state);
	return result_code;
}
