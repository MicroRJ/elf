/*
** See Copyright Notice In elf.h
** elf_api.h
** Main user API
*/


elNumber elf_tonum(elValue v) {
	return v.tag == TAG_INT ? (elNumber) v.i : v.n;
}


elInteger elf_toint(elValue v) {
	return v.tag == TAG_NUM ? (elInteger) v.n : v.i;
}


elf_api elObject *elf_getthis(elState *R);

elf_api elf_tag elf_gettag(elState *R, elRegId x);
elf_api elValue elf_getany(elState *R, elRegId x);
elf_api elInteger elf_getint(elState *R, elRegId x);
elf_api elNumber elf_getnum(elState *R, elRegId x);
elf_api elString *elf_getstr(elState *R, elRegId x);
elf_api elObject *elf_getobj(elState *R, elRegId x);
elf_api elTable *elf_gettab(elState *R, elRegId x);
elf_api elHandle elf_getsys(elState *R, elRegId x);
elf_api elClosure *elf_getcls(elState *R, elRegId x);


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
elf_api int elf_loadcodefs(elState *, elFileState *fs, elString *filename, elRegId rxy, int ny, char *contents);
elf_api int elf_loadexprfs(elState *, elFileState *fs, elString *filename, elRegId rxy, int ny, char *contents);
elf_api int elf_loadfilefs(elState *, elFileState *fs, elString *filename, elRegId rxy, int ny);

elf_api int elf_loadcode(elState *, elString *filename, elRegId rxy, int ny, char *contents);
elf_api int elf_loadexpr(elState *, elString *filename, elRegId rxy, int ny, char *contents);
elf_api int elf_loadfile(elState *, elString *filename, elRegId rxy, int ny);


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
elf_api int elf_callex(elState *, elObject *obj, elRegId rx, elRegId ry, int nx, int ny);


/*
** Performs a root call, where rx and ry are the same
** and obj is nil.
*/
elf_api int elf_callfn(elState *, elRegId rx, int nx, int ny);


elf_api int elf_run(elState *);


elf_api void elf_checkcl(elState *c, elRegId x);

elf_api elValue *elf_gettop(elState *R);
elf_api void elf_settop(elState *R, elValue *top);

elf_api elRegId elf_pushmany(elState *R, int howmany);
elf_api elRegId elf_pushany(elState *, elValue v);
elf_api void elf_pushnil(elState *);
elf_api void elf_pushint(elState *, elInteger i);
elf_api void elf_pushnum(elState *, elNumber n);
elf_api void elf_pushsys(elState *c, elHandle h);

elf_api elString *elf_pushstr(elState *, elString *s);
elf_api elString *elf_pushnewstr(elState *, char *c);
elf_api elString *elf_pushnewstrlen(elState *, elInteger len);

elf_api elObject *elf_pushobj(elState *, elObject *t);
elf_api elObject *elf_pushnewobj(elState *, elInteger tell);

elf_api elTable *elf_pushtab(elState *, elTable *t);
elf_api elTable *elf_pushnewtab(elState *R);
elf_api elTable *elf_pushnewlen(elState *R, elInteger len);

elf_api elRegId elf_pushcls(elState *, elClosure *f);
elf_api elRegId elf_pushnewcls(elState *, elProto fn);


elf_api elRegId elf_pushbinding(elState *, elBinding c);

