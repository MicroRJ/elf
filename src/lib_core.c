/*
** See Copyright Notice In elf.h
** lib_core.c
*/

#include "lib_file.c"


static int _exec(elf_State *R, bool as_expr, int nargs, int nrets, elf_String *name, elf_String *contents) {
	ASSERT(contents);
	ASSERT(name);

	// todo: parse and gen file should just be one function,
	// or maybe just have the one 'parse' function...
	elf_Parser parser = {};
	treeID tree = elf_parse(&parser,R,as_expr,name->text,contents->text);
	elf_File file = gen_file(&parser,tree);

	// todo: we need this in the file for debugging...
	file.contents = contents;
	file.name = name;

	// todo: how do we track this, should each proto
	// point to the file they are from?...
	elf_array_add(R->M->globals,VSTR(contents));
	elf_array_add(R->M->globals,VSTR(name));

	ARRAY_ADD(R->M->files,file);
	elf_Closure *cls = elf_new_closure(R,file.proto);

	// ASSERT(nargs >= 0);
	// ASSERT(nrets >= 0);
	// todo: ensure the closure doesn't actually have to be
	// kept alive...
	// elf_set_global(R->M,0,VCLS(cls));

	elf_Value *rets = R->stack_ptr;
	elf_push_closure(R,cls);
	elf_push_this(R);
	nrets = elf_call(R,nargs+1,nrets);
	R->stack_ptr = rets + nrets;
	esc:
	return nrets;
}

#include "lib_meta.c"

// mostly experimental...
int core_lib_float2(elf_State *R) {
	elf_Value v={elf_tag_float2};
	v.x_f32=(float)elf_get_num(R,0);
	v.y_f32=v.x_f32;
	if (elf_get_num_args(R)>1){
		v.y_f32=(float)elf_get_num(R,1);
	}
	*R->stack_ptr++=v;
	return 1;
}
/* merges one or several tables together into
a new table, which is then returned. */
int core_lib_merge_tables(elf_State *R) {
	elf_Table *tab = elf_new_table(R);
	int i;
	for (i = 0; i < elf_get_num_args(R); i += 1) {
		elf_merge_tables(tab,elf_get_table(R,i));
	}
	return 1;
}


int core_lib_get_meta(elf_State *R) {
	elf_push_table(R,elf_get_obj(R,0)->meta);
	return 1;
}


int core_lib_set_meta(elf_State *R) {
	elf_get_obj(R,0)->meta=elf_get_table(R,1);
	PUSHV(R,elf_get_arg(R,0));
	return 1;
}


/*  */
int core_lib_abort(elf_State *R) {
	if(1) abort();
	return 0;
}


int core_lib_exit(elf_State *R) {
	if(1) exit(elf_get_int(R,0));
	return 0;
}


/* debugging */
int core_lib_flags(elf_State *R) {
	int flags = R->flags;
	R->flags |= elf_get_int(R,0);
	elf_push_int(R,flags);
	return 1;
}


int core_lib_debugger(elf_State *R) {
#if defined(_DEBUG)
	R->flags |= FLAG_DEBUGGER;
#else
	char *message = "no message";
	if (elf_get_num_args(R) != 0) {
		message = elf_get_text(R,0);
	}
	elf_debugger(message);
#endif
	return 0;
}


int core_lib_log(elf_State *R) {
	FOR_RANGE(i,0,elf_get_num_args(R)) {
		fpf_value(stdout,elf_get_arg(R,i),0);
	}
	fprintf(stdout,"\n");
	return 0;
}


int core_lib_err_log(elf_State *R) {
	FOR_RANGE(i,0,elf_get_num_args(R)) {
		fpf_value(stderr,elf_get_arg(R,i),0);
	}
	fprintf(stderr,"\n");
	return 0;
}


static char *in_sym_dir(char *dir, char *sym) {
	do {
		if (*dir ++ != *sym ++) {
			return *sym == '.' ? dir : sym;
		}
	} while (*dir);
	return sym;
}


/* the dot is added so that symbols like elf.math.floor
are not mistaken with table accesses when shortened,
math.floor != .math.floor */
int core_lib_include(elf_State *R) {
	elf_check_args(R,".include",1,"(the directory to include to add to the global directory)");
	char *dir = elf_get_text(R,0);
	int plen = text_length(dir);
	/* accumulate all symbols here first to
	avoid faulting under repeating patterns:
	elf.ray.elf.ray could include the symbol
	many more times when the new key is added
	as we traverse the array. the new key is
	encountered and we keep repeating the
	process... this would override the previous
	value and result in erroneous behavior.
	todo: */

	elf_Table *globals = R->M->globals;
	elf_Entry entry;
	FOR_RANGE(i,0,globals->ntotal) {
		entry=globals->slots[i];
		if (entry.key.tag == elf_tag_str) {
			char *sym = in_sym_dir(dir,entry.key.x_str->text);
			if (*sym != '.') continue;
			elf_String *ref = elf_alloc_string(R,sym);
			elf_table_set(globals,VSTR(ref),globals->array[entry.idx]);
		}
	}
	return 0;
}


int core_lib_load_json(elf_State *R) {
	char *name = elf_get_text(R,0);

	FILE *file = fopen(name,"rb");
	if (file == 0) goto _error;

	fseek(file,0,SEEK_END);
	long size = ftell(file);

	elf_String *contents = elf_new_string2(R,size);

	fseek(file,0,SEEK_SET);
	fread(contents->text,1,size,file);

	elf_Parser parser = {};
	elf_prep_parser(&parser, R, name, contents->text);

	elf_tabID tab = parse_json_obj(&parser);
	elf_push_table(R,tab);

	return 1;
	_error:
	elf_push_nil(R);
	return 1;
}



int core_lib_fpf(elf_State *S) {
	elf_Handle file = elf_get_sysobj(S,0);
	int wrote = 0;
	for (int i = 1; i < elf_get_num_args(S); i ++) {
		wrote += fpf_value(file,elf_get_arg(S,i),0);
	}
	elf_push_int(S,wrote);
	return 1;
}

#if 0
int core_lib_lpf(elf_State *S) {
	for (int i = 0; i < elf_get_num_args(S); i ++) {
		if (i != 0) fprintf(stdout,"\n");
		for (int j = 0; j < pf_indent; ++ j) {
			fprintf(stdout, "  ");
		}
		fpf_value(stdout,elf_get_arg(S,i),0);
	}
	fprintf(stdout,"\n");
	return 0;
}
#endif

int core_lib_pf(elf_State *S) {
	#if 0
	for (int j = 0; j < pf_indent; ++ j) {
		fprintf(stdout,"  ");
	}
	#endif
	for (int i = 0; i < elf_get_num_args(S); i ++) {
		fpf_value(stdout,elf_get_arg(S,i),0);
	}
	fprintf(stdout,"\n");
	return 0;
}


int core_lib_sleep(elf_State *S) {
	ASSERT(elf_get_num_args(S) >= 1);
	sys_sleep(elf_get_int(S,0));
	return 0;
}

static void print_num_tabs(FILE *io, int num) {
	while (num --) fprintf(io,"\t");
}

// todo: move this to core.c?
void elf_unload(FILE *io, elf_Table *tab, int level) {
	fprintf(io,"{");
	int nitems = 0;
	// todo:
	if(tab->nslots) {
		for (elf_i64 i = 0; i < tab->ntotal; ++ i) {
			elf_Entry slot = tab->slots[i];
			if (slot.key.tag == elf_tag_nil) {
				continue;
			}
			elf_Value v = tab->array[slot.idx];
			if ((v.tag == elf_tag_closure) || (v.tag == elf_tag_proc)) {
				continue;
			}
			if (nitems ++ != 0) fprintf(io,",");
			fpf_value(io,slot.key,1);
			fprintf(io," = ");
			if (v.tag == elf_tag_tab) {
				elf_unload(io,v.x_tab,level+1);
			} else {
				fpf_value(io,v,1);
			}
		}
	} else {
		FOR_ARRAY(i,tab->array) {
			elf_Value v = tab->array[i];
			if (i != 0) fprintf(io,", ");
			if (v.tag==elf_tag_tab) {
				elf_unload(io,v.x_tab,level+1);
			} else {
				fpf_value(io,v,1);
			}
		}
	}
	fprintf(io,"}");
}


int core_lib_unload(elf_State *S) {
	elf_Handle io = elf_get_sysobj(S,0);
	elf_Table *tab = elf_get_table(S,1);
	elf_unload(io,tab,0);
	return 0;
}


// DEPRECATED SHOULD BE INTRINSIC
int core_lib_iton(elf_State *R) {
	elf_Value v = elf_get_arg(R,0);
	if (v.tag==elf_tag_int) {
		elf_push_number(R,(elf_Num)v.x_int);
	} else elf_push_number(R,v.x_num);
	return 1;
}


// DEPRECATED SHOULD BE INTRINSIC
int core_lib_ntoi(elf_State *R) {
	elf_Value v = elf_get_arg(R,0);
	if (v.tag==elf_tag_num) {
		elf_push_int(R,(elf_Int)v.x_num);
	} else elf_push_int(R,v.x_int);
	return 1;
}