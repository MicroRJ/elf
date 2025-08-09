//
// See Copyright Notice In elf.h
//


#define elf_rawapi



#define tisobject(tag) ((tag) >= ELF_TUSER)

// convert from integer/number to number/integer,
// ** assumes the value is either an integer or a number **
#define vitonum(v) (isint(v) ? (elf_Number)  vgetint(v) : vgetnum(v))
#define vntoint(v) (isnum(v) ? (elf_Integer) vgetnum(v) : vgetint(v))

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


// an object tag with nullptr should never really happen!
#define isnil(v) (((v).tag == ELF_TNIL) || (tisobject((v).tag) && (v).x_obj == 0))
#define isdead(v) ((v).tag == ELF_TNIL || (v).tag == ELF_TTOMB)
#define isnum(v) ((v).tag == ELF_TNUMBER)
#define isint(v) ((v).tag == ELF_TINTEGER)
#define istab(v) ((v).tag == ELF_TTABLE)
#define isstr(v) ((v).tag == ELF_TSTRING)
#define isusr(v) ((v).tag == ELF_TUSER)
#define isfnc(v) ((v).tag == ELF_TFUNCTION)
#define iscls(v) ((v).tag == ELF_TCLOSURE)
#define issys(v) ((v).tag == ELF_THANDLE)


#define isnumeric(v) (isnum(v) || isint(v))
#define iscallable(v) (iscls(v) || isfnc(v))

#define isobj(v) (tisobject((v).tag))


#define vgettag(v) ((v).tag)
#define vgetint(v) ((v).x_int)
#define vgetnum(v) ((v).x_num)
#define vgetobj(v) ((v).x_obj)
#define vgetstr(v) ((v).x_str)
#define vgettab(v) ((v).x_tab)
#define vgetsys(v) ((v).x_sys)
#define vgetcls(v) ((v).x_closure)
#define vgetfnc(v) ((v).x_proc)
#define vgettext(v) ((v).x_str->text)


#define tagcheck(S, v, t) do { if (v.tag != t) \
{ 	elf_errorf(S, -1, "'%s': expected '%s'", tag2s[v.tag], tag2s[t]); } } while (0)


#define loadtop(S, x) ((S)->stack_ptr[(x)])
#define stackcursor(S) ((S)->stack_ptr - (S)->stack)



// todo: we loose type check!
static inline void *checkptr(void *obj) {
	ASSERT(obj != 0);
	return obj;
}

#define vsetobjf(v,x) ((v)->x_obj=checkptr(x))

#define vsetnil(v)   ((v)->tag=ELF_TNIL     , (v)->x_int=0)
#define vsetint(v,x) ((v)->tag=ELF_TINTEGER , (v)->x_int=x)
#define vsetnum(v,x) ((v)->tag=ELF_TNUMBER  , (v)->x_num=x)
#define vsetsys(v,x) ((v)->tag=ELF_THANDLE  , (v)->x_sys=x)
#define vsetstr(v,x) ((v)->tag=ELF_TSTRING  , vsetobjf(v, x))
#define vsettab(v,x) ((v)->tag=ELF_TTABLE   , vsetobjf(v, x))
#define vsetfnc(v,x) ((v)->tag=ELF_TFUNCTION, vsetobjf(v, x))
#define vsetcls(v,x) ((v)->tag=ELF_TCLOSURE , vsetobjf(v, x))


#define settopnil(S)   vsetnil((S)->stack_ptr)
#define settopint(S,x) vsetint((S)->stack_ptr, x)
#define settopnum(S,x) vsetnum((S)->stack_ptr, x)
#define settopsys(S,x) vsetsys((S)->stack_ptr, x)
#define settopstr(S,x) vsetstr((S)->stack_ptr, x)
#define settoptab(S,x) vsettab((S)->stack_ptr, x)
#define settopfnc(S,x) vsetfnc((S)->stack_ptr, x)
#define settopcls(S,x) vsetcls((S)->stack_ptr, x)
#define settopsys(S,x) vsetsys((S)->stack_ptr, x)



// todo: we rely on virtual address space to reserve a large memory range
// and never have to worry about this
#define pushstackunsafe(S) ((S)->stack_ptr ++)


// @todo: mark check unlikely
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


#define pushnil(S)   do { settopnil(S);    pushstackunsafe(S);  } while(0)
#define pushint(S,x) do { settopint(S, x); pushstackunsafe(S);  } while(0)
#define pushnum(S,x) do { settopnum(S, x); pushstackunsafe(S);  } while(0)
#define pushstr(S,x) do { settopstr(S, x); pushstackunsafe(S);  } while(0)
#define pushtab(S,x) do { settoptab(S, x); pushstackunsafe(S);  } while(0)
#define pushfun(S,x) do { settopfnc(S, x); pushstackunsafe(S);  } while(0)
#define pushcls(S,x) do { settopcls(S, x); pushstackunsafe(S);  } while(0)
#define pushsys(S,x) do { settopsys(S, x); pushstackunsafe(S);  } while(0)



#define pushvalue(S, v) do { *(S)->stack_ptr ++ = v; } while (0)


static inline tabID pushtable(elf_State *S) {
	tabID tab = elf_alloc_table(S);
	settoptab(S, tab);
	pushstacksafe(S);
	return tab;
}





