/*
** See Copyright Notice In elf.h
** elf-obj.h
** ...
*/


typedef enum elGCColor {
	GC_BLACK = 0, GC_WHITE, GC_PINK, GC_RED,
} elGCColor;



/* first object tag must be OBJECT, all other
objects come after it */
#define TAGLIST(_) \
_(NIL) _(GCD) _(SYS) \
_(INT) _(NUM) _(BID) \
_(OBJ) _(CLS) _(STR) _(TAB) /* end */



typedef enum elObjType {
	OBJ_NONE = 0,
	OBJ_CLOSURE,
	OBJ_STRING,
	OBJ_ARRAY,
	OBJ_TAB,
	OBJ_CUSTOM,
} elObjType;


typedef struct elObject {
	elObjType type;
	elGCColor color;
	elTable *metatable;
	short tell;
} elObject;



#define TAGENUM(NAME) XFUSE(TAG_,NAME),

typedef enum elObjectTag {

	TAGLIST(TAGENUM)

} elObjectTag;

#undef TAGENUM


#define TAGENUM(NAME) XSTRINGIFY(NAME),
elf_globaldecl char const *tag2s[] = {
	TAGLIST(TAGENUM)
};
#undef TAGENUM


typedef struct elValue {
	elObjectTag tag;
	union {
		elAddr p,x_ptr;
		elHandle h,x_sys;
		elBinding c;
		elInteger i,x_int;
		elNumber n,x_num;
		elClosure *f,*x_cls;
		elObject *j,*x_obj;
		elTable *t,*x_tab;
		elString *s,*x_str;
	};
} elValue;


typedef struct elClosure {
	elObject obj;
   /* todo: encode prototye in the instruction stream? */
	elProto   fn;
	elByteId  j;
	elValue enclosure[1];
} elClosure;


elf_api elValue elf_table_value(elTable *);
elf_api elValue elf_binding_value(elBinding);
elf_api elValue elf_string_value(elString *);
elf_api elValue elf_closure_value(elClosure *);
elf_api elValue elf_integer_value(elInteger i);
elf_api elValue elf_number_value(elNumber n);
elf_api elValue elf_nil_value();



