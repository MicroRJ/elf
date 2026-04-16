//
// See Copyright Notice In elf.h
//

static inline void check_trap_reference(elf_State *state, GCRef reference)
{
	if (reference->status & NODE_DEBUGTRAP)
	{
		reporterrorf(state, -1, "'%p': trapped object found", reference);
	}
}

static inline bool is_readonly(GCRef ref)
{
	return ref->status & NODE_READONLY;
}

static void checkwrite(elf_State *S, GCRef ref)
{
	if (is_readonly(ref))
	{
		reporterrorf(S, -1, "attempted to write to readonly object");
	}
}

static inline void swap_values(V *x, V *y)
{
	V temp = *x;
	*x = *y;
	*y = temp;
}

static inline void vmove(V *dst, V src)
{
	copy_memory(dst, &src, sizeof(src));
}

static inline void copy_values(V *dst, V *src, int num)
{
	copy_memory(dst, src, num * sizeof(*src));
}

static inline void zero_values(V *dst, int num)
{
	zero_memory(dst, num * sizeof(V));
}

#define getmeta(o) ((o)->obj.meta)
#define setmeta(o, m) ((o)->obj.meta = (m))

#define tisobject(tag) ((tag) >= ELF_VALUE_TYPE_USER_OBJECT)
#define tisdead(v) ((v) == ELF_VALUE_TYPE_NIL || (v) == ELF_VALUE_TYPE_TOMB)
#define tisnil(v) ((v) == ELF_VALUE_TYPE_NIL)
#define tisnum(v) ((v) == ELF_VALUE_TYPE_NUMBER)
#define tisint(v) ((v) == ELF_VALUE_TYPE_INTEGER)
#define tistab(v) ((v) == ELF_VALUE_TYPE_TABLE)
#define is_string_type(v) ((v) == ELF_VALUE_TYPE_STRING)
#define tisusr(v) ((v) == ELF_VALUE_TYPE_USER_OBJECT)
#define tisfnc(v) ((v) == ELF_VALUE_TYPE_CFUNCTION)
#define tiscls(v) ((v) == ELF_VALUE_TYPE_CLOSURE)
#define tissys(v) ((v) == ELF_VALUE_TYPE_HANDLE)
#define is_numeric_type(v) (tisnum(v) || tisint(v))
#define tiscallable(v) (tiscls(v) || tisfnc(v))


// nullptr should never happen! it should be converted to nil
#define is_nil(v) ((v).tag == ELF_VALUE_TYPE_NIL)
#define vistomb(v) ((v).tag == ELF_VALUE_TYPE_TOMB)
#define is_num(v) ((v).tag == ELF_VALUE_TYPE_NUMBER)
#define is_int(v) ((v).tag == ELF_VALUE_TYPE_INTEGER)
#define is_tab(v) ((v).tag == ELF_VALUE_TYPE_TABLE)
#define is_str(v) ((v).tag == ELF_VALUE_TYPE_STRING)
#define is_buf(v) ((v).tag == ELF_VALUE_TYPE_BUFFER)
#define isusr(v) ((v).tag == ELF_VALUE_TYPE_USER_OBJECT)
#define is_function(v) ((v).tag == ELF_VALUE_TYPE_CFUNCTION)
#define is_closure(v) ((v).tag == ELF_VALUE_TYPE_CLOSURE)
#define issys(v) ((v).tag == ELF_VALUE_TYPE_HANDLE)

#define isdead(v) (is_nil(v) || vistomb(v))
#define iskey(v) (!is_nil(v) && !vistomb(v))
#define is_numeric(v) (is_num(v) || is_int(v))
#define is_callable(v) (is_closure(v) || is_function(v))
#define value_is_reference(v) (tisobject((v).tag))


// ** assumes the value is either an integer or a number **
#define int_to_num(v) (is_int(v) ? (Num) as_int(v) : as_num(v))
#define num_to_int(v) (is_num(v) ? (Int) as_num(v) : as_int(v))




#define tag_of(v) ((v).tag)
#define as_int(v) ((v).x_int)
#define as_num(v) ((v).x_num)
#define reference_from_value(v) ((v).x_obj)
#define as_string(v) ((v).x_str)
#define as_buffer(v) ((v).x_buf)
#define table_from_value(v) ((v).x_tab)
#define vgetsys(v) ((v).x_sys)
#define closure_from_value(v) ((v).x_closure)
#define function_from_value(v) ((v).x_proc)


#define vtag2s(v) tag2s[tag_of(v)]






// null objects must be replaced with nil
static inline void *checknullptrobj(void *obj) {
	ASSERT(obj != 0);
	return obj;
}





#define to_nil(v)   ((v)->tag=ELF_VALUE_TYPE_NIL     , (v)->x_int=0)
#define to_int(v,x) ((v)->tag=ELF_VALUE_TYPE_INTEGER , (v)->x_int=x)
#define to_num(v,x) ((v)->tag=ELF_VALUE_TYPE_NUMBER  , (v)->x_num=x)
#define to_sys(v,x) ((v)->tag=ELF_VALUE_TYPE_HANDLE  , (v)->x_sys=x)




#define set_obj(v,x) ((v)->x_obj=checknullptrobj(x))



static inline void to_str(V *v, GCStr x) {
	v->tag=ELF_VALUE_TYPE_STRING;
	set_obj(v, x);
}

static inline void to_tab(V *v, Tab x) {
	v->tag=ELF_VALUE_TYPE_TABLE;
	set_obj(v, x);
}



static inline void to_fun(V *v, Fun x) {
	v->tag=ELF_VALUE_TYPE_CFUNCTION;
	set_obj(v, x);
}



static inline void to_cls(V *v, Closure x) {
	v->tag=ELF_VALUE_TYPE_CLOSURE;
	set_obj(v, x);
}




static inline void to_buf(V *v, Buf x) {
	v->tag=ELF_VALUE_TYPE_BUFFER;
	set_obj(v, x);
}






#define loadtype(S, x) (tag_of(loadvalue(S, x)))
#define get_num_args(S) ((S)->frame.nargs)





static inline void checkloadindex(elf_State *S, int x)
{
	if (x < 0 || x >= get_num_args(S)) {
		reporterrorf(S, -1, "invalid argument index: %i, got: %i", x, get_num_args(S));
	}
}





#define stack2index(S) ((S)->stack_ptr - (S)->stack)







// todo: we rely on virtual address space to reserve a large memory range
// and never have to worry about this
#define pushstackunsafe(S) ((S)->stack_ptr ++)


// todo: mark check unlikely
#define pushstacksafe(S) \
do { \
	\
	if (((S)->stack_ptr - (S)->stack) >= S->stack_size) { \
		reporterrorf(S, -1, "ran out of stack space"); \
	} \
	\
	pushstackunsafe(S); \
	\
} while (0)




#define pushvalueunsafe(S, v) (*(S)->stack_ptr ++ = (v))
#define popvalue(S) (* -- (S)->stack_ptr)






#define pushthis(S)  (loadpush(S, 0))




static inline void push_nil(elf_State *S) {
	to_nil(S->stack_ptr);
	pushstackunsafe(S);
}








static inline void pushbuf(elf_State *S, Buf x) {
	to_buf(S->stack_ptr, x);
	pushstackunsafe(S);
}








static inline void pushint(elf_State *S, Int x) {
	to_int(S->stack_ptr, x);
	pushstackunsafe(S);
}



static inline void pushnum(elf_State *S, Num x) {
	to_num(S->stack_ptr, x);
	pushstackunsafe(S);
}



static inline void pushsys(elf_State *S, Sys x) {
	to_sys(S->stack_ptr, x);
	pushstackunsafe(S);
}



static inline void push_closure(elf_State *S, Closure x) {
	to_cls(S->stack_ptr, x);
	pushstackunsafe(S);
}



static inline void pushstr(elf_State *S, GCStr x) {
	ASSERT(x != 0);
	to_str(S->stack_ptr, x);
	pushstackunsafe(S);
}



static inline void pushtab(elf_State *S, Tab x) {
	to_tab(S->stack_ptr, x);
	pushstackunsafe(S);
}



static inline void pushfun(elf_State *S, Fun x) {
	to_fun(S->stack_ptr, x);
	pushstackunsafe(S);
}







// all loads come from here!
static inline V loadvalue(elf_State *S, int x)
{
	checkloadindex(S, x);
	return S->frame.framebase[x];
}

//
//
//
//

static inline void loadpush(elf_State *S, int x)
{
	V v = loadvalue(S, x);
	pushvalueunsafe(S, v);
}







static inline void loadtypeerror(elf_State *S, int type, int x)
{
	reporterrorf(S, -1

	, 			"argument %i has type '%s'; expected '%s' value"

	, x, tag2s[loadtype(S, x)], tag2s[type]);
}






static inline void typecheck(elf_State *S, V v, int tag) {
	if (v.tag != tag) {
		reporterrorf(S, -1
		, "type error, expected '%s', got '%s'", tag2s[tag], tag2s[v.tag]);
	}
}

//
//
//
//

static inline void typerulecheck(elf_State *S, V v, TypeRule trule) {
	if (~trule & 1 << v.tag) {

		// todo: !!!
		char const *trule_name = "";

		reporterrorf(S, -1
		, "'%s': type rule violation, got '%s'", trule_name, tag2s[v.tag]);
	}
}


static inline V loadrulecheck(elf_State *S, int x, TypeRule rule)
{
	V v = loadvalue(S, x);
	typerulecheck(S, v, rule);
	return v;
}


static inline bool popbool(elf_State *S)
{
	V v = popvalue(S);
	if (is_nil(v)) return false;
	typecheck(S, v, ELF_VALUE_TYPE_INTEGER);
	return as_int(v);
}


static inline Int popint(elf_State *S)
{
	V v = popvalue(S);
	typecheck(S, v, ELF_VALUE_TYPE_INTEGER);
	return as_int(v);
}




static inline Buf loadbuf(elf_State *S, int x)
{
	V v = loadvalue(S, x);
	if (is_buf(v)) return as_buffer(v);
	// if (is_nil(v)) return 0;
	loadtypeerror(S, ELF_VALUE_TYPE_BUFFER, x);
	return 0;
}




static inline GCStr loadstr(elf_State *S, int x)
{
	V v = loadvalue(S, x);
	if (is_str(v)) return as_string(v);
	// if (is_nil(v)) return 0;
	loadtypeerror(S, ELF_VALUE_TYPE_STRING, x);
	return 0;
}






static inline Int loadsys(elf_State *S, int x)
{
	V v = loadvalue(S, x);
	if (issys(v)) return vgetsys(v);
	loadtypeerror(S, ELF_VALUE_TYPE_HANDLE, x);
	return 0;
}






static inline Int loadint(elf_State *S, int x)
{
	V v = loadvalue(S, x);
	if (is_numeric(v)) return num_to_int(v);
	loadtypeerror(S, ELF_VALUE_TYPE_INTEGER, x);
	return 0;
}






static inline Num loadnum(elf_State *S, int x)
{
	V v = loadvalue(S, x);
	if (is_numeric(v)) return int_to_num(v);
	loadtypeerror(S, ELF_VALUE_TYPE_NUMBER, x);
	return 0;
}






static inline V loadnumeric(elf_State *S, int x) {
	V v = loadrulecheck(S, x, TRULE_NUMERIC);
	return v;
}






static inline V loadcallable(elf_State *S, int x) {
	V v = loadrulecheck(S, x, TRULE_CALLABLE);
	return v;
}








static inline GCRef load_reference(elf_State *S, int x)
{
	V v = loadvalue(S, x);
	if (value_is_reference(v)) return reference_from_value(v);
	// if (is_nil(v)) return 0;
	loadtypeerror(S, TRULE_OBJECT, x);
	return 0;
}







static inline Tab loadtable(elf_State *S, int x)
{
	V v = loadvalue(S, x);
	if (is_tab(v)) return table_from_value(v);
	// if (is_nil(v)) return 0;
	loadtypeerror(S, ELF_VALUE_TYPE_TABLE, x);
	return 0;
}


// todo: loadtext and loadmem could be replace with loadtextv()
// and loadtextv returns a text view

// todo: instead return a text view or something
// text_view { const char *text; int length; }
static inline const char *loadtext(elf_State *S, int x)
{
	V v = loadvalue(S, x);
	if (is_str(v)) return strt(as_string(v));
	if (is_buf(v)) return as_buffer(v)->mem;
	// todo: we allow nil here...
	if (is_nil(v)) return 0;

	loadtypeerror(S, ELF_VALUE_TYPE_STRING, x);
	return 0;
}

// todo: instead return a text view or something
// text_view { const char *text; int length; }
static inline void *loadmem(elf_State *S, int x, Int *zmem) {
	void *mem = 0;
	*zmem = 0;

	V v = loadvalue(S, x);
	if (is_buf(v)) {
		*zmem = as_buffer(v)->min;
		mem = as_buffer(v)->mem;
	}
	else if (is_str(v)) {
		*zmem = as_string(v)->length;
		mem = as_string(v)->text;
	}
	else {
		loadrulecheck(S, x, TRULE_STRING|TRULE_BUFFER);
	}
	return mem;
}






static inline GCStr pushtext(elf_State *S, char const *text) {
	GCStr str = new_string_from_data(S, text);
	pushstr(S, str);
	return str;
}





static inline GCStr pushtext2(elf_State *S, char const *text, int length) {
	GCStr str = new_string_from_data_size(S, text, length);
	pushstr(S, str);
	return str;
}





static inline Tab push_new_table(elf_State *S) {
	Tab tab = new_table(S);
	pushtab(S, tab);
	return tab;
}





