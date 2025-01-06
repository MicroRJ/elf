/*
** See Copyright Notice In elf.h
** ir.h
*/

#define SPECIAL_REGISTER_THIS   (  0) // #this
#define SPECIAL_REGISTER_INDEX  (256) // #index
#define SPECIAL_REGISTER_VALUE  (257) // #value
#define SPECIAL_REGISTER_ARRAY  (258) // #array

#define IRDEF(_) \
_(NOP)\
_(AND)_(OR)_(NIL_AND)_(NIL_OR)\
_(EQ)_(NEQ)_(LT)_(GT)_(LTEQ)_(GTEQ)\
_(ADD)_(SUB)_(MUL)_(DIV)\
_(BIT_SHL)_(BIT_SHR)\
_(INDEX)_(FIELD)\
_(BIT_AND)_(BIT_OR)_(BIT_XOR)\
_(MOD)_(POW)\
_(TYPEGUARD)\
_(STORE)\
_(CLOSURE) _(STRING) _(TABLE) \
_(INTEGER) _(NUMBER) _(NIL) \
_(GLOBAL) _(LOCAL) _(CLSVAL) _(FILE_VALUE) \
_(MULTI)\
_(METAFIELD)\
_(CALL)\
_(RANGE_INDEX)\
_(RANGE)\
_(PUSH_MEMORY_STATE)\
_(POP_MEMORY_STATE)\
_(BASIC_BLOCK)\
_(GOTO)\
_(IF)\
_(PARAM)\
_(YIELD)\
_(LOAD)\
_(LOAD_DIRECT)\
/* end */

// static treeT get_tree(Parser *fs, treeID id);
// static treeKi get_tree_kind(Parser *fs, treeID id);
// static treeTy get_tree_type(Parser *fs, treeID id);
// static Source get_tree_line(Parser *fs, treeID id);
// static treeID tree_xyz(Parser *fs, Source, treeKi k, treeTy ty, treeID x, treeID y, treeID *z);
// static treeID tree_xy(Parser *fs, Source, treeKi k, treeTy ty, treeID x, treeID y);
// static treeID tree_x(Parser *fs, Source, treeKi k, treeTy ty, treeID x);
// static treeID tree_nil(Parser *fs, Source);
// static treeID tree_int(Parser *fs, Source, elf_Int i);
// static static treeID tree_num(Parser *fs, Source, elf_Num n);
// static treeID tree_str(Parser *fs, Source, Source);
// static treeID tree_nullary(Parser *fs, Source, treeKi k, treeTy t);
// static treeID tree_group(Parser *fs, Source, treeID x);
// static treeID tree_table(Parser *fs, Source, treeID *z);
// static static treeID tree_closure(Parser *fs, Source, treeID x, treeID *z);
// static treeID tree_store(Parser *fs, Source line, treeID x, treeID y);

// static static treeID tree_ret(Parser *fs, Source, treeID i);
// static static treeID tree_param(Parser *fs, Source, treeID i);
// static treeID tree_global_ref(Parser *fs, Source line, treeID i);

// /* Todo: deprecate */
// static treeID tree_local(Parser *fs, Source line, treeID i);

// static static treeID tree_this_ref(Parser *fs, Source line);
// static treeID tree_closure_value(Parser *fs, Source line, treeID i);
// static treeID tree_type_guard(Parser *fs, Source line, treeID x, treeTy y);
// static treeID tree_metafield(Parser *fs, Source line, treeID x, treeID y);
// static treeID tree_field(Parser *fs, Source line, treeID x, treeID y);
// static treeID tree_index(Parser *fs, Source line, treeID x, treeID y);
// static treeID tree_ranged_index(Parser *fs, Source line, treeID x, treeID y);
// static treeID tree_call(Parser *fs, Source line, treeID x, treeID *z);
// static treeID tree_less_than(Parser *fs, Source line, treeID x, treeID y);
// static treeID tree_call_metafield(Parser *fs, Source line, treeID x, treeID *z, char *name);
// static treeID tree_multi(Parser *fs, Source line, treeID *z);
// static treeID tree_global_ref_by_name(Parser *fs, Source line, char *name);
// static treeID tree_call_pf(Parser *fs, Source line, treeID *args);
// static treeID tree_call_set_metatable(Parser *fs, Source line, treeID object, treeID metatable);

// static static treeID tree_block(Parser *fs, Source line, treeID src, treeID end);


// static elf_ValueTag node2tag(treeTy ty);
// static ByteOP ir2b(treeKi tt);
// static elf_Bool tree_is_lvalue(treeKi kind);