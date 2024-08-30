/*
** See Copyright Notice In elf.h
** help.h
*/


#define IS_TOBJ(tag) ((tag) >= TAG_OBJ)
#define IS_TNUM(tag) ((tag) == TAG_NUM || (tag) == TAG_INT)
#define IS_TCALL(tag) ((tag) == TAG_CLS || (tag) == TAG_CFN)


#define IS_NIL_OBJ(val) (IS_TOBJ((val).tag) && (val).x_obj == 0)

#define IS_VNIL(val) ((val).tag == TAG_NIL || IS_NIL_OBJ(val))

#define VI2N(val) ((val).tag == TAG_INT ? (elf_Num)  (val).x_int : (val).x_num)
#define VN2I(val) ((val).tag == TAG_NUM ? (elf_Int) (val).x_num : (val).x_int)


#define TO_OBJ(thing) ((elf_Object*)(thing))
#define OBJ_COLOR(thing) (TO_OBJ(thing)->color)


#define GET_LOCAL(S,X) (elGETFRAME(S)->locals[X])

#define OBJ2V(ty) (TAG_OBJ+ty)

#define GET_TOP(S)   ((S)->stack_ptr)
#define SET_TOP(S,X) (GET_TOP(S) = UCAST(X, elf_Value *))
#define PUSHV(S,X)   (* GET_TOP(S) ++ = (X))
#define elGETFRAME(S) ((S)->frame)


