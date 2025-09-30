//
// See Copyright Notice In elf.h
//

// value intrinsics



#define TOKENTOTEXT(x) #x




#define __lteq(x, y) ((x) <= (y))
#define __lt(x, y)   ((x) <  (y))

#define __shl_i(x, y) ((x) << (y))
#define __shr_i(x, y) ((x) >> (y))
#define __and_i(x, y) ((x)  & (y))
#define __ior_i(x, y) ((x)  | (y))
#define __eor_i(x, y) ((x)  ^ (y))


#define __add_i(x, y)  ((x) + (y))
#define __add_r __add_i


#define __sub_i(x, y)  ((x) - (y))
#define __sub_r __sub_i


#define __mul_i(x, y)  ((x) * (y))
#define __mul_r __mul_i


#define __div_i(x, y)  ((x) / (y))
#define __div_r __div_i


#define __pow_i(x, y) (pow((x), (y)))
#define __pow_r __pow_i


#define __mod_i(x, y) ((x) % (y))
#define __mod_r(x, y) ((x) -  ((Int) ((x) / (y))) * (y))






//
// int_arithmetic(int, int): int
//

#define IARITH_DEF(_) _(shl)_(shr)_(and)_(ior)_(eor)

//
// num_arithmetic(int, int): int
// num_arithmetic(int, num): num
// num_arithmetic(num, int): num
// num_arithmetic(num, num): num
// num_arithmetic(obj, any): any
//

#define NARITH_DEF(_) _(pow)_(mod)_(mul)_(div)_(add)_(sub)

//
// relational(int, num): int
// relational(num, int): int
// relational(num, num): int
// relational(int, int): int
//

#define RELDEF(_) _(lt)_(lteq)

//
//
//
//

#define EQDEF(_) _(eq)

//
//
//
//

#define INTRINDEF(_) NARITH_DEF(_) IARITH_DEF(_) RELDEF(_) EQDEF(_)






enum {
	INTRIN__nop = 0,

#define INTRIN(NAME) INTRIN__##NAME,
	INTRINDEF(INTRIN)
#undef INTRIN
};



static const char *intrin2s[] = {
	[INTRIN__nop] = "__none",

#define INTRIN(NAME) [INTRIN__##NAME] = TOKENTOTEXT(__##NAME),
	INTRINDEF(INTRIN)
#undef INTRIN
};






static void _error_invalid_operands(elf_State *S, int sig, V x, V y) {
	reporterrorf(S, -1, "invalid operands for intrinsic: %s, %s", tag2s[x.tag], tag2s[y.tag]);
}



static void _error_invalid_value_for_get_metatable(elf_State *S, V v) {
	reporterrorf(S, -1, "'%s': invalid value for _get_metatable", tag2s[v.tag]);
}



static void _error_required_metatable_for_get_metafield(elf_State *S, V v) {
	reporterrorf(S, -1, "'%s': required metatable for get_metafield", tag2s[v.tag]);
}



static void _error_field_is_nil_for_get_field(elf_State *S) {
	reporterror(S, -1, "_get_field, nil is not a valid field");
}



static void _error_cannot_call(elf_State *S, V x) {
	reporterrorf(S, -1, "cannot call '%s'", tag2s[x.tag]);
}






// todo: extend this signature system?
// doesn't have to be fast, only used once signature validation code fails
static const char *_get_field_signatures[] = {
	"_get_field(x: str, y: int): int",
	"_get_field(x: str, y: str): int",
	"_get_field(x: tab, y: any): any",
	"_get_field(x: usr, y: any): any",
};

//
//
//
//
//
//
//
//

static void _error_get_field_invalid_arguments(elf_State *S, V x, V y) {

 	// todo: make better! push_note!
	Stringer sb = {};
	for (int i = 0; i < COUNTOF(_get_field_signatures); ++ i) {
		sb_writetext(&sb, _get_field_signatures[i]);
		sb_writetext(&sb, "\n");
	}


	reporterrorf(S, -1, "_get_field(%s, %s), no such overload found, overloads:\n%s"
	, tag2s[x.tag], tag2s[y.tag], sb.buf);

	sb_free(&sb);
}

//
//
//
//
//
//
//
//

static inline Tab _get_metatable(elf_State *S, V v) {
	Tab metatable = 0;

	switch (v.tag) {

		case ELF_TBUFFER:
		case ELF_TSTRING:
		case ELF_TTABLE:
		case ELF_TUSER:
		case ELF_TCLOSURE: {
			metatable = as_ref(v)->meta;
		} break;

		case ELF_TNUMBER: {
			metatable = S->metatables.number;
		} break;

		case ELF_TINTEGER: {
			metatable = S->metatables.integer;
		} break;

		default: {
			_error_invalid_value_for_get_metatable(S, v);
		} break;
	}


	return metatable;
}

//
//
//
//
//
//
//
//

static inline V _get_metafield(elf_State *S, V x, V y) {
	Tab metatable = _get_metatable(S, x);
	if (!metatable) {
		_error_required_metatable_for_get_metafield(S, x);
	}
	return _table_getornil(S, metatable, y);
}

//
//
//
//
//
//
//
//

// todo: implement user overload!
static inline void _get_field(elf_State *S, V *x, V y, V z) {

	// nil is always invalid!
	if (is_nil(z)) {
		_error_field_is_nil_for_get_field(S);
	}


	switch (y.tag) {

		case ELF_TTABLE: {
			*x = _table_getornil(S, as_table(y), z);
		} break;

		case ELF_TSTRING: {

			const char *text = strt(as_string(y));

			if (is_int(z)) {

				int index = as_int(z);
				to_int(x, text[index]);

			}
			else if (is_str(z)) {

				const char *subtext = strt(as_string(z));

				int index = find_subtext((char *) text, (char *) subtext);
				to_int(x, index);

			}
			else {
				goto _err;
			}
		} break;

		default: {
			_err:
			_error_get_field_invalid_arguments(S, y, z);
		} break;
	}
}

//
//
//
//
//
//
//
//

// todo:
static int metaintrin(elf_State *S, int intrin, V *x, V y, V z) {

	V *restore = S->stack_ptr;


	// todo: !!!
	Str name = _string_new(S, intrin2s[intrin]);


	Ref ref = as_ref(y);

	if (!ref->meta) {
		reporterrorf(S, NO_BYTE, "'%s': missing meta-table", strt(name));
	}


	V metaname;
	to_str(&metaname, name);

	V metafield = _get_metafield(S, y, metaname);

	if (!is_callable(metafield)) {
		_error_cannot_call(S, metafield);
	}


	// push the [function, left-operand ('this'), right-operand]
	pushvalueunsafe(S, metafield);
	pushvalueunsafe(S, y);
	pushvalueunsafe(S, z);

	// todo: inline call!
	int nrets = elf_call(S, 2, 1);

	if (nrets != 1) {
		reporterrorf(S, -1, "'%s': invalid number of returns for overload, expected only 1", name);
	}

	*x = S->stack_ptr[-nrets];

	S->stack_ptr = restore;

	return nrets;
}

//
//
//
//

#define INTARITHFUNC(NAME)                                 \
static inline void v__##NAME(elf_State *S, V *x, V y, V z) \
{                                                          \
	if (is_int(y) && is_int(z)) {                           \
		to_int(x, __##NAME##_i(as_int(y), as_int(z)));       \
	}                                                       \
	else {                                                  \
		_error_invalid_operands(S, INTRIN__##NAME, y, z);    \
	}                                                       \
}                                                          \
/* end */

//
//
//
//

#define ARITHFUNC(NAME)                                                       \
static inline void v__##NAME (elf_State *S, V *x, V y, V z)                   \
{                                                                             \
	if (is_numeric(y) && is_numeric(z)) {                                      \
		if (is_num(y) || is_num(z)) {                                           \
			to_num(x, __##NAME##_r(int_to_num(y), int_to_num(z)));               \
		}                                                                       \
		else {                                                                  \
			to_int(x, __##NAME##_i(as_int(y), as_int(z)));                       \
		}                                                                       \
	}                                                                          \
	else if (is_ref(y)) {                                                      \
		metaintrin(S, INTRIN__##NAME, x, y, z);                                 \
	}                                                                          \
	else {                                                                     \
		_error_invalid_operands(S, INTRIN__##NAME, y, z);                       \
	}                                                                          \
}                                                                             \
/* end */

//
//
//
//

#define RELFUNC(NAME)                                            \
static inline void v__##NAME(elf_State *S, V *x, V y, V z) {     \
	if (is_numeric(y) && is_numeric(z)) {                         \
		if (is_num(y) || is_num(z)) {                              \
			to_int(x, __##NAME(int_to_num(y), int_to_num(z)));      \
		}                                                          \
		else {                                                     \
			to_int(x, __##NAME(as_int(y), as_int(z)));              \
		}                                                          \
	}                                                             \
	else if (is_ref(y)) {                                         \
		goto _err;                                                 \
	}                                                             \
	else {                                                        \
		_err:                                                      \
		_error_invalid_operands(S, INTRIN__##NAME, y, z);          \
	}                                                             \
}                                                                \
/* end */






NARITH_DEF(ARITHFUNC)
IARITH_DEF(INTARITHFUNC)
RELDEF(RELFUNC)

//
//
//
//
//
//
//
//

static inline bool __eq(elf_State *S, V x, V y) {
	bool eq = 0;

	if (is_numeric(x) && is_numeric(y)) {

		if (is_num(x) || is_num(y)) {
			eq = int_to_num(x) == int_to_num(y);
		}
		else {
			eq = as_int(x) == as_int(y);
		}
	}
	else if (tag_of(x) == tag_of(y)) {

		if (is_str(x)) {
			eq = streq(as_string(x), as_string(y));
		}
		else {
			eq = as_int(x) == as_int(y);
		}
	}

	return eq;
}

//
//
//
//

static inline void v__eq(elf_State *S, V *x, V y, V z) {
	to_int(x, __eq(S, y, z));
}

//
//
//
//

static inline void v__neq(elf_State *S, V *x, V y, V z) {
	to_int(x, !__eq(S, y, z));
}
