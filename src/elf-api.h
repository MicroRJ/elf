/*
** See Copyright Notice In elf.h
** elf_api.h
** Main user API
*/


elf_num elf_tonum(elf_Value v) {
	return v.tag == TAG_INT ? (elf_num) v.i : v.n;
}


elf_int elf_toint(elf_Value v) {
	return v.tag == TAG_NUM ? (elf_int) v.n : v.i;
}


elf_api elf_Object *elf_getthis(elf_State *R);

elf_api elf_Value elf_getany(elf_State *R, elf_localid x);
elf_api elf_int elf_getint(elf_State *R, elf_localid x);
elf_api elf_num elf_getnum(elf_State *R, elf_localid x);
elf_api elf_String *elf_getstr(elf_State *R, elf_localid x);
elf_api elf_Object *elf_getobj(elf_State *R, elf_localid x);
elf_api elf_Table *elf_gettab(elf_State *R, elf_localid x);
elf_api elf_Handle elf_getsys(elf_State *R, elf_localid x);
elf_api elf_Closure *elf_getcls(elf_State *R, elf_localid x);


/*
** The following set of functions are very similar
** and have the same semantics, the only difference
** are the paramters.
** - filename: is the name of the file to load from
** disk or the label you wish to attach the code.
** - rxy: is the register from which to read inputs
** and to which to write outputs.
**
**   Loads elf [....] and calls its function.
** loadcodefs: [code]
** loadexprfs: [expr]
** loadfilefs: [file]
*/
elf_api int elf_loadcodefs(elf_State *, elf_FileState *fs, elf_String *filename, elf_localid rxy, int ny, char *contents);
elf_api int elf_loadexprfs(elf_State *, elf_FileState *fs, elf_String *filename, elf_localid rxy, int ny, char *contents);
elf_api int elf_loadfilefs(elf_State *, elf_FileState *fs, elf_String *filename, elf_localid rxy, int ny);

elf_api int elf_loadcode(elf_State *, elf_String *filename, elf_localid rxy, int ny, char *contents);
elf_api int elf_loadexpr(elf_State *, elf_String *filename, elf_localid rxy, int ny, char *contents);
elf_api int elf_loadfile(elf_State *, elf_String *filename, elf_localid rxy, int ny);


/*
** Ultimately, calls a function of any kind.
** Takes an optional object for meta calls,
** nx and ny are the number of in and out
** values respectively.
** nx does not include the function nor the
** optional object.
** rx is the frame register, the function
** should reside in that register at call
** time. arguments should come after that
** register.
** ry is the first yield register, where
** the results are written to.
** ry can be equal to rx.
*/
elf_api int elf_callex(elf_State *, elf_Object *obj, elf_localid rx, elf_localid ry, int nx, int ny);


/*
** Performs a root call, where rx and ry are the same
** and obj is nil.
*/
elf_api int elf_callfn(elf_State *, elf_localid rx, int nx, int ny);


elf_api int elf_run(elf_State *);


elf_api void elf_checkcl(elf_State *c, elf_localid x);
elf_api elf_String *elf_checkstr(elf_State *c, elf_localid x);


elf_api elf_Value *elf_gettop(elf_State *R);
elf_api void elf_settop(elf_State *R, elf_Value *top);

elf_api elf_localid elf_pushmany(elf_State *R, int howmany);
elf_api elf_localid elf_pushany(elf_State *, elf_Value v);
elf_api void elf_pushnil(elf_State *);
elf_api void elf_pushint(elf_State *, elf_int i);
elf_api void elf_pushnum(elf_State *, elf_num n);
elf_api void elf_pushsys(elf_State *c, elf_Handle h);

elf_api elf_String *elf_pushstr(elf_State *, elf_String *s);
elf_api elf_String *elf_pushnewstr(elf_State *, char *c);
elf_api elf_String *elf_pushnewstrlen(elf_State *, elf_int len);

elf_api elf_Object *elf_pushobj(elf_State *, elf_Object *t);
elf_api elf_Object *elf_pushnewobj(elf_State *, elf_int tell);

elf_api elf_Table *elf_pushtab(elf_State *, elf_Table *t);
elf_api elf_Table *elf_pushnewtab(elf_State *R);
elf_api elf_Table *elf_pushnewlen(elf_State *R, elf_int len);

elf_api elf_localid elf_pushcls(elf_State *, elf_Closure *f);
elf_api elf_localid elf_pushnewcls(elf_State *, elf_Proto fn);


elf_api elf_localid elf_pushbinding(elf_State *, lBinding c);

