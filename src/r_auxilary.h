//
// See Copyright Notice In elf.h
//

#define tisobject(tag) ((tag) >= elf_tag_UserObject)

// convert from integer/number to number/integer, assumes the value
// is either an integer or a number
#define vitonum(v) (((v).tag == elf_tag_Int) ? (elf_Number) (v).x_int : (v).x_num)
#define vntoint(v) (((v).tag == elf_tag_Num) ? (elf_Int)    (v).x_num : (v).x_int)

#define isnum(v) ((v).tag == elf_tag_Num)
#define isint(v) ((v).tag == elf_tag_Int)
#define istab(v) ((v).tag == elf_tag_Table)
#define isstr(v) ((v).tag == elf_tag_String)
#define isusr(v) ((v).tag == elf_tag_UserObject)

#define isnumeric(v) (isnum(v) || isint(v))

#define isobject(v) (tisobject((v).tag))

// an object tag with nullptr should never really happen!
#define isnil(v) (((v).tag == elf_tag_Nil) || (tisobject((v).tag) && (v).x_obj == 0))

#define vgetint(v) ((v).x_int)
#define vgetobj(v) ((v).x_obj)
#define vgetstr(v) ((v).x_str)
#define vgettab(v) ((v).x_tab)

#define vsetnil(v) ((v)->tag=elf_tag_Nil,(v)->x_int=0)
#define vsetint(v,x) ((v)->tag=elf_tag_Int,(v)->x_int=x)
#define vsetnum(v,x) ((v)->tag=elf_tag_Num,(v)->x_num=x)
#define vsetstr(v,x) ((v)->tag=elf_tag_String,(v)->x_str=x)
#define vSetClosure(v,x) ((v)->tag=elf_tag_Closure,(v)->x_closure=x)






#define POBJ(thing) (&(thing)->obj)
#define OBJ2V(ty) (elf_tag_UserObject+ty)



// todo: remove!
static void _debug_stack_push(elf_State *S, elf_Value v);

// todo: remove this!
#if defined(_DEBUG)
#define PUSHV(S,X) _debug_stack_push(S,X)
#else
#define PUSHV(S,X) (* GET_TOP(S) ++ = (X))
#endif


#define VALUE_NIL()           (XLITERAL(elf_Value){ elf_tag_Nil                                                 })
#define VALUE_NUMBER(thing)   (XLITERAL(elf_Value){ elf_tag_Num        , ((union { elf_Number _; float __; elf_Int I; }){thing}).I })
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



// elf_debugger(__FILE__" ["XTEXT(__LINE__)"]: internal error: unexpected code branch")


#define NO_BYTE (-1)

// #define CHUNKSIZE 1024
// #define CHUNKCATE(x,y) ((x+y-1)/y*y)

#define FLYTRAP 0x55555555