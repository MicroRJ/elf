/*
** See Copyright Notice In elf.h
** elf-obj.h
** Objects And Values
*/


typedef enum elf_objgc {
	GC_BLACK = 0, GC_WHITE, GC_PINK, GC_RED,
} elf_objgc;



/* first object tag must be OBJECT, all other
objects come after it */
#define TAGLIST(_) \
_(NIL) _(GCD) _(SYS) \
_(INT) _(NUM) _(BID) \
_(OBJ) _(CLS) _(STR) _(TAB) /* end */



typedef enum elf_objty {
	OBJ_NONE = 0,
	OBJ_CLOSURE,
	OBJ_STRING,
	OBJ_ARRAY,
	OBJ_TAB,
	OBJ_CUSTOM,
} elf_objty;


typedef struct elObject {
	// TODO: REMOVE THIS
#if defined(_DEBUG)
	int headtrap;
#endif
	elf_objty type;
	elf_objgc gccolor;
	// TODO: REMOVE THIS
	elInteger tell;
	elTable *metatable;
	// TODO: REMOVE THIS
#if defined(_DEBUG)
	int tailtrap;
#endif
} elObject;



#define TAGENUM(NAME) XFUSE(TAG_,NAME),
typedef enum elf_tag {
	TAGLIST(TAGENUM)
} elf_tag;
#undef TAGENUM


#define TAGENUM(NAME) XSTRINGIFY(NAME),
elf_globaldecl char const *tag2s[] = {
	TAGLIST(TAGENUM)
};
#undef TAGENUM


typedef struct elValue {
	elf_tag tag;
	union {
		elAddr           p,x_ptr;
		elHandle    h;
		elBinding      c;
		elInteger   	  i,x_int;
		elNumber   	  n,x_num;
		elf_Closure  *f,*x_cls;
		elObject   *j,*x_obj;
		elTable    *t,*x_tab;
		elString   *s,*x_str;
	};
} elValue;


typedef struct elf_Closure {
	elObject obj;
   /* I guess one of the things we could do
   if we ever get to having multi-byte encoding,
   is encode the entire prototype in the
   instruction stream since most closures are
   anonymous, given how the language works.
   If not, then there's no need to store the
   whole prototype here, we can instead store an
   index into the proto table. */
	elProto   fn;
	elf_byteid     j;
   /* allocated past this point */
	elValue caches[1];
} elf_Closure;


elf_api elValue elf_valtab(elTable *);
elf_api elValue elf_valbid(elBinding);
elf_api elValue elf_valstr(elString *);
elf_api elValue elf_valcls(elf_Closure *);
elf_api elValue elf_valint(elInteger i);
elf_api elValue elf_valnum(elNumber n);



