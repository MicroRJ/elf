//
// See Copyright Notice In elf.h
//


#define elf_rawapi



static inline void copy_values(V *dst, V *src, int num) {
	copy_memory(dst, src, num * sizeof(V));
}



static inline void zero_values(V *dst, int num) {
	zero_memory(dst, num * sizeof(V));
}



#define getmeta(o) ((o)->obj.meta)
#define setmeta(o, m) ((o)->obj.meta = (m))

#define tisobject(tag) ((tag) >= ELF_TUSER)
#define tisdead(v) ((v) == ELF_TNIL || (v) == ELF_TTOMB)
#define tisnil(v) ((v) == ELF_TNIL)
#define tisnum(v) ((v) == ELF_TNUMBER)
#define tisint(v) ((v) == ELF_TINTEGER)
#define tistab(v) ((v) == ELF_TTABLE)
#define tisstr(v) ((v) == ELF_TSTRING)
#define tisusr(v) ((v) == ELF_TUSER)
#define tisfnc(v) ((v) == ELF_TFUNCTION)
#define tiscls(v) ((v) == ELF_TCLOSURE)
#define tissys(v) ((v) == ELF_THANDLE)
#define tisnumeric(v) (tisnum(v) || tisint(v))
#define tiscallable(v) (tiscls(v) || tisfnc(v))


// an object tag with nullptr should never happen!
// (tisobject((v).tag) && (v).x_obj == 0)
//
#define visnil(v) ((v).tag == ELF_TNIL)
#define vistomb(v) ((v).tag == ELF_TTOMB)
#define isnum(v) ((v).tag == ELF_TNUMBER)
#define isint(v) ((v).tag == ELF_TINTEGER)
#define vistab(v) ((v).tag == ELF_TTABLE)
#define visstr(v) ((v).tag == ELF_TSTRING)
#define isusr(v) ((v).tag == ELF_TUSER)
#define isfnc(v) ((v).tag == ELF_TFUNCTION)
#define iscls(v) ((v).tag == ELF_TCLOSURE)
#define issys(v) ((v).tag == ELF_THANDLE)

#define isdead(v) (visnil(v) || vistomb(v))
#define visnumeric(v) (isnum(v) || isint(v))
#define iscallable(v) (iscls(v) || isfnc(v))
#define isobj(v) (tisobject((v).tag))


// ** assumes the value is either an integer or a number **
#define vitonum(v) (isint(v) ? (elf_Number) vgetint(v) : vgetnum(v))
#define vntoint(v) (isnum(v) ? (elf_Integer) vgetnum(v) : vgetint(v))




#define vtagof(v) ((v).tag)
#define vgetint(v) ((v).x_int)
#define vgetnum(v) ((v).x_num)
#define vgetobj(v) ((v).x_obj)
#define vgetstr(v) ((v).x_str)
#define vgettab(v) ((v).x_tab)
#define vgetsys(v) ((v).x_sys)
#define vgetcls(v) ((v).x_closure)
#define vgetfnc(v) ((v).x_proc)
#define vgetstrd(v) ((v).x_str->text)


#define vtag2s(v) tag2s[vtagof(v)]


#define vcheck(S, v, t) do { if (v.tag != t) \
{ 	elf_errorf(S, -1, "'%s': expected '%s'", tag2s[v.tag], tag2s[t]); } } while (0)






#define stack2index(S) ((S)->stack_ptr - (S)->stack)



// todo: we loose type check!

// null objects must be replaced with nil
static inline void *checkptr(void *obj) {
	ASSERT(obj != 0);
	return obj;
}


// invalid handles must be replaced with nil
static inline Handle checksys(Handle obj) {
	ASSERT(!ELF_HISINVALID(obj));
	return obj;
}



#define vsetobjf(v,x) ((v)->x_obj=checkptr(x))
#define vsetsysf(v,x) ((v)->x_sys=checksys(x))


#define vsetnil(v)   ((v)->tag=ELF_TNIL     , (v)->x_int=0)
#define vsetint(v,x) ((v)->tag=ELF_TINTEGER , (v)->x_int=x)
#define vsetnum(v,x) ((v)->tag=ELF_TNUMBER  , (v)->x_num=x)
#define vsetsys(v,x) ((v)->tag=ELF_THANDLE  , vsetsysf(v, x))
#define vsetstr(v,x) ((v)->tag=ELF_TSTRING  , vsetobjf(v, x))
#define vsettab(v,x) ((v)->tag=ELF_TTABLE   , vsetobjf(v, x))
#define vsetfnc(v,x) ((v)->tag=ELF_TFUNCTION, vsetobjf(v, x))
#define vsetcls(v,x) ((v)->tag=ELF_TCLOSURE , vsetobjf(v, x))





// todo: we rely on virtual address space to reserve a large memory range
// and never have to worry about this
#define pushstackunsafe(S) ((S)->stack_ptr ++)


// todo: mark check unlikely
#define pushstacksafe(S) \
do { \
	\
	if (((S)->stack_ptr - (S)->stack) >= S->stack_max) { \
		elf_errorf(S, -1, "ran out of stack space"); \
	} \
	\
	pushstackunsafe(S); \
	\
} while (0)


#define loadvalue(S, x) ((S)->frame.framebase[x])

#define pushvalueunsafe(S, v) (*(S)->stack_ptr ++ = (v))
#define popvalue(S) (* -- (S)->stack_ptr)


#define loadpush(S, x) (pushvalueunsafe(S, loadvalue(S, x)))

#define pushthis(S)  (loadpush(S, 0))

#define pushnil(S)   do { vsetnil((S)->stack_ptr);    pushstackunsafe(S);  } while(0)

#define pushint(S,x) do { vsetint((S)->stack_ptr, x); pushstackunsafe(S);  } while(0)
#define pushnum(S,x) do { vsetnum((S)->stack_ptr, x); pushstackunsafe(S);  } while(0)

#define pushsys(S,x) do { vsetsys((S)->stack_ptr, x); pushstackunsafe(S);  } while(0)

#define pushcls(S,x) do { vsetcls((S)->stack_ptr, x); pushstackunsafe(S);  } while(0)
#define pushstr(S,x) do { vsetstr((S)->stack_ptr, x); pushstackunsafe(S);  } while(0)
#define pushtab(S,x) do { vsettab((S)->stack_ptr, x); pushstackunsafe(S);  } while(0)

#define pushfun(S,x) do { vsetfnc((S)->stack_ptr, x); pushstackunsafe(S);  } while(0)






#define loadtype(S, x) (vtagof(loadvalue(S, x)))



#define loadtypeerror(S, t, x) do { elf_errorf(S, -1, "'%s': expected '%s' for argument %i", tag2s[loadtype(S, x)], tag2s[t], x); } while (0)


static inline const char *loadtext(elf_State *S, int x)
{
	V v = loadvalue(S, x);
	if (visstr(v)) return vgetstrd(v);
	if (visnil(v)) return 0;
	loadtypeerror(S, ELF_TSTRING, x);
	return 0;
}



static inline Str loadstr(elf_State *S, int x)
{
	V v = loadvalue(S, x);
	if (visstr(v)) return vgetstr(v);
	if (visnil(v)) return 0;
	loadtypeerror(S, ELF_TSTRING, x);
	return 0;
}



static inline Int loadsys(elf_State *S, int x)
{
	V v = loadvalue(S, x);
	if (issys(v)) return vgetsys(v);
	loadtypeerror(S, ELF_THANDLE, x);
	return 0;
}



static inline Int loadint(elf_State *S, int x)
{
	V v = loadvalue(S, x);
	if (visnumeric(v)) return vntoint(v);
	loadtypeerror(S, ELF_TINTEGER, x);
	return 0;
}



static inline Num loadnum(elf_State *S, int x)
{
	V v = loadvalue(S, x);
	if (visnumeric(v)) return vitonum(v);
	loadtypeerror(S, ELF_TNUMBER, x);
	return 0;
}



static inline Tab loadtable(elf_State *S, int x)
{
	V v = loadvalue(S, x);
	if (vistab(v)) return vgettab(v);
	if (visnil(v)) return 0;
	loadtypeerror(S, ELF_TTABLE, x);
	return 0;
}



static inline Str pushtext(elf_State *S, char const *text) {
	Str str = newstr(S, text);
	pushstr(S, str);
	return str;
}



static inline Str pushtext2(elf_State *S, char const *text, int length) {
	Str str = newstrl(S, text, length);
	pushstr(S, str);
	return str;
}



static inline Tab pushtable(elf_State *S) {
	Tab tab = new_table(S);
	pushtab(S, tab);
	return tab;
}





