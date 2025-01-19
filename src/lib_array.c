/*
** See Copyright Notice In elf.h
** lib_array.c
*/


int elf_array_lib_tally(elf_State *R) {
	elf_tabID tab = (elf_tabID) elf_get_this(R);
	elf_push_int(R,ARRAY_LENGTH(tab->array));
	return 1;
}

int elf_array_lib_get(elf_State *R) {
	elf_tabID tab = (elf_tabID) elf_get_this(R);
	elf_i64 len = ARRAY_LENGTH(tab->array);
	elf_Value value = VNIL();
	if (len != 0) {
		for (int i = 0; i < elf_get_num_args(R); ++ i) {
			if (i != 0) {
				if (value.tag == elf_tag_nil) {
					elf_fail(R,NO_BYTE,"nil object");
				}
				/* todo: please do much better error reporting
				here, this can be hard to figure out */
				if (value.tag != elf_tag_tab) {
					elf_fail(R,NO_BYTE,"not a table");
				}
				if (tab == 0) {
					elf_fail(R,NO_BYTE,"nil object");
				}
			}

			elf_i64 idx = elf_get_int(R,i);
			if ((idx %= len) < 0) idx += len;

			value = tab->array[idx];
			tab = value.x_tab;
		}
	}
	PUSHV(R,value);
	return 1;
}

int elf_lib_array_add(elf_State *R) {
	elf_tabID tab=(elf_tabID)elf_get_this(R);
	int i;
	for (i=0;i<elf_get_num_args(R);i++) {
		elf_array_add(tab,elf_get_arg(R,i));
	}
	return 0;
}

int elf_lib_array_set(elf_State *R) {
	elf_check_args(R,":array_set",2,"the value, and the index where to place the value");

	elf_tabID tab = (elf_tabID) elf_get_this(R);
	elf_Value value = elf_get_arg(R,0);

	elf_i64 len=ARRAY_LENGTH(tab);
	elf_i64 idx=elf_get_int(R,1);
	if ((idx%=len)<0)idx+=len;
	tab->array[idx] = value;
	return 0;
}


int elf_lib_array_swap(elf_State *R) {
	elf_check_args(R,":array_swap",2,"the two indexes to swap");
	elf_tabID tab = (elf_tabID) elf_get_this(R);
	elf_i64 x = elf_get_int(R,0);
	elf_i64 y = elf_get_int(R,1);
	elf_Value temp = tab->array[x];
	tab->array[x] = tab->array[y];
	tab->array[y] = temp;
	return 0;
}

int lib_array_merge(elf_State *R) {
	elf_check_args(R,":array_merge",1
	, "takes: the array to merge, all values of the arrays"
	" are added into a new array");
	elf_tabID tab = (elf_tabID) elf_get_this(R);
	elf_tabID add = elf_get_table(R,0);
	elf_tabID res = elf_new_table(R);
	elf_i64 i;
	for (i=0;i<ARRAY_LENGTH(tab->array);++i) {
		ARRAY_ADD(res->array,tab->array[i]);
	}
	for (i=0;i<ARRAY_LENGTH(add->array);++i) {
		ARRAY_ADD(res->array,add->array[i]);
	}
	return 1;
}

int elf_array_lib_clone(elf_State *R) {
	elf_check_args(R,":xclone",0,"");
	elf_tabID tab = (elf_tabID ) elf_get_this(R);
	elf_tabID clone = elf_alloc_table(R);
	elf_i64 i;
	for ( i = 0; i < ARRAY_LENGTH(tab->array); i += 1 ) {
		ARRAY_ADD(clone->array,tab->array[i]);
	}
	elf_push_table(R,clone);
	return 1;
}

int elf_lib_array_reverse(elf_State *R) {
	elf_check_args(R,":reverse",0,"");
	elf_Table *tab = (elf_Table *) elf_get_this(R);
	elf_Int n = ARRAY_LENGTH(tab->array);
	elf_Int i;
	elf_Value *array = tab->array;
	for (i = 0; i < n >> 1; i += 1) {
		elf_Value value = array[i];
		array[i] = array[n-1-i];
		array[n-1-i] = value;
	}
	return 0;
}


int elf_lib_array_slice(elf_State *R) {
	elf_Table *tab = (elf_Table *) elf_get_this(R);
	elf_Int x = 0;
	elf_Int y = ARRAY_LENGTH(tab->array);
	if (elf_get_num_args(R) >= 1) x = elf_get_int(R,0);
	if (elf_get_num_args(R) >= 2) y = elf_get_int(R,1);
	elf_Table *slice = elf_new_table(R);
	while (x < y) {
		elf_array_add(slice,tab->array[x ++]);
	}
	return 1;
}

