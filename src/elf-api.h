/*
** See Copyright Notice In elf.h
** elf_api.h
** Main user API
*/

elf_api int elf_get_num_args(elState *R);
elf_api elObject *elf_get_this(elState *R);
elf_api elValueTag elf_get_tag(elState *R, elRegId x);
elf_api elValue elf_get_value(elState *R, elRegId x);
elf_api elInteger elf_get_integer(elState *R, elRegId x);
elf_api elNumber elf_get_number(elState *R, elRegId x);
elf_api elString *elf_get_string(elState *R, elRegId x);
elf_api elObject *elf_get_object(elState *R, elRegId x);
elf_api elTable *elf_get_table(elState *R, elRegId x);
elf_api elHandle elf_get_handle(elState *R, elRegId x);
elf_api elClosure *elf_get_closure(elState *R, elRegId x);


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
elf_api int elf_call_function(elState *, elRegId rx, int nx, int ny);


elf_api int elf_run(elState *);


elf_api void elf_checkcl(elState *c, elRegId x);

elf_api elValue *elf_get_stack_top(elState *R);
elf_api void elf_set_stack_top(elState *R, elValue *top);

elf_api elRegId elf_pushmany(elState *R, int howmany);
elf_api elRegId elf_add_value(elState *, elValue v);
elf_api void eld_add_nil(elState *);
elf_api void elf_add_integer(elState *, elInteger i);
elf_api void elf_add_number(elState *, elNumber n);
elf_api void elf_pushsys(elState *c, elHandle h);

elf_api elString *elf_add_string(elState *, elString *s);
elf_api elString *elf_add_new_string(elState *, char *c);
elf_api elString *elf_pushnewstrlen(elState *, elInteger len);

elf_api elObject *elf_add_object(elState *, elObject *t);
elf_api elObject *elf_pushnewobj(elState *, elInteger tell);

elf_api elTable *elf_add_table(elState *, elTable *t);
elf_api elTable *elf_add_new_table(elState *R);
elf_api elTable *elf_pushnewlen(elState *R, elInteger len);

elf_api elRegId elf_add_closure(elState *, elClosure *f);
elf_api elRegId elf_pushnewcls(elState *, elProto fn);


elf_api elRegId elf_pushbinding(elState *, elBinding c);

