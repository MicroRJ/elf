//
// See Copyright Notice In elf.h
//

#define IS_OBJ_TAG(tag) ((tag) >= elf_tag_UserObject)

#define IS_INT_OR_NUM(tag) (((tag) == elf_tag_Num) || ((tag) == elf_tag_Int))

#define IS_NIL_VALUE(X) (((X).tag == elf_tag_Nil) || (IS_OBJ_TAG((X).tag) && (X).x_obj == 0))

#define VI2N(X) (((X).tag==elf_tag_Int) ? (elf_Num) (X).x_int : (X).x_num)
#define VN2I(X) (((X).tag==elf_tag_Num) ? (elf_Int) (X).x_num : (X).x_int)


#define POBJ(thing) (&(thing)->obj)
#define OBJ_COLOR(thing) (POBJ(thing)->color)


#define OBJ2V(ty) (elf_tag_UserObject+ty)


#define GET_FRAME(S) ((S)->frame)
#define GET_LOCAL(S,X) (GET_FRAME(S)->locals[X])


#define GET_TOP(S)   ((S)->stack_ptr)
#define SET_TOP(S,X) (GET_TOP(S) = UCAST(X, elf_Value *))

static void _debug_stack_push(elf_State *S, elf_Value v);

// todo: remove this!
#if defined(_DEBUG)
#define PUSHV(S,X) _debug_stack_push(S,X)
#else
#define PUSHV(S,X) (* GET_TOP(S) ++ = (X))
#endif


#define VALUE_NIL()           (XLITERAL(elf_Value){ elf_tag_Nil                                                 })
#define VALUE_NUMBER(thing)   (XLITERAL(elf_Value){ elf_tag_Num        , ((union { elf_Num _; float __; elf_Int I; }){thing}).I })
#define VALUE_INTEGER(thing)  (XLITERAL(elf_Value){ elf_tag_Int        , {(elf_Int) UCAST(thing, elf_Int)      }})
#define VALUE_TABLE(thing)    (XLITERAL(elf_Value){ elf_tag_Table        , {(elf_Int) UCAST(thing, elf_Table *)  }})
#define VALUE_OBJECT(thing)   (XLITERAL(elf_Value){ OBJ2V(thing->type) , {(elf_Int) UCAST(thing, elf_Object *) }})
#define VALUE_STRING(thing)   (XLITERAL(elf_Value){ elf_tag_String        , {(elf_Int) UCAST(thing, elf_String *) }})
#define VALUE_CLOSURE(thing)  (XLITERAL(elf_Value){ elf_tag_Closure    , {(elf_Int) UCAST(thing, elf_Closure *)}})
#define VALUE_FUNCTION(thing) (XLITERAL(elf_Value){ elf_tag_Function       , {(elf_Int) UCAST(thing, elf_Function) }})
#define VALUE_HANDLE(thing)   (XLITERAL(elf_Value){ elf_tag_Handle     , {(elf_Int) UCAST(thing, elf_Handle)   }})




//
// TODO: REMOVE THIS FROM HERE
//
#define slot2value(T,X) (T->array[T->slots[X].idx])
#define slotiskey(T,X) ((X >= 0) && (T->slots[X].key.tag != elf_tag_Nil) && (T->slots[X].key.tag != elf_tag_Tomb))



// elf_debugger(__FILE__" ["XTEXT(__LINE__)"]: internal error: unexpected code branch")


#define NO_BYTE (-1)

// #define CHUNKSIZE 1024
// #define CHUNKCATE(x,y) ((x+y-1)/y*y)

#define FLYTRAP 0x55555555