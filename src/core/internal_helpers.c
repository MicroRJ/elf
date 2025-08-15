//
// See Copyright Notice In elf.h
//



static inline elf_String *topstr(elf_State *S, int x) {
	elf_Value v = loadtop(S, x);
	if (visnil(v)) return 0;
	vcheck(S, v, ELF_TSTRING);
	return vgetstr(v);
}





static inline elf_Value f_checkobj(elf_State *S, int x, elf_Tag tag) {
	elf_Value v = S->frame.framebase[x + 1];
	if (v.tag != tag && v.tag != ELF_TNIL) {
		elf_errorf(S, -1, "'%s': expected '%s'", tag2s[v.tag], tag2s[tag]);
	}
	return v;
}



static inline elf_String *f_checkstr(elf_State *S, int x) {
	elf_Value v = f_checkobj(S, x, ELF_TSTRING);
	if (visnil(v)) return 0;
	return vgetstr(v);
}



static inline char *f_checktext(elf_State *S, int x) {
	elf_Value v = f_checkobj(S, x, ELF_TSTRING);
	if (visnil(v)) return 0;
	return vgetstrd(v);
}



static inline elf_Table *f_checktable(elf_State *S, int x) {
	elf_Value v = f_checkobj(S, x, ELF_TTABLE);
	if (visnil(v)) return 0;
	return vgettab(v);
}



static inline elf_Handle f_checkhand(elf_State *S, int x) {
	elf_Value v = f_checkobj(S, x, ELF_THANDLE);
	if (visnil(v)) return 0;
	return vgetsys(v);
}



static inline elf_Integer f_checkint(elf_State *S, int x) {
	elf_Value v = loadvalue(S, x + 1);
	if (!visnumeric(v)) {
		return 0;
	}
	return vntoint(v);
}



