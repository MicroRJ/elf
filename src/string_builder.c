//
// See Copyright Notice In elf.h
//

#ifndef STRING_BUILDER_H
#define STRING_BUILDER_H


#include <malloc.h>


typedef struct
{
	int   max;
	int   min;
	char *buf;
}
Stringer;

//
//
//
//

static inline void sb_free(Stringer *sb) {
	free(sb->buf);
	sb->buf = 0;
	sb->min = 0;
	sb->max = 0;
}

//
//
//
//

static char *sb_alloc(Stringer *sb, int res, int com) {
	if (sb->min + res > sb->max) {
		sb->max <<= 1;
		if(sb->min + res > sb->max) {
			sb->max = sb->min + res;
		}
		sb->buf = realloc(sb->buf, sb->max);
	}
	char *ret = sb->buf + sb->min;
	sb->min += com;
	return ret;
}

//
//
//
//

static inline char *sb_writechar(Stringer *sb, int chr) {
	char *mem = sb->buf + sb->min;

	char *text = sb_alloc(sb, 1 + 1, 1);

	*text ++ = chr;
	*text ++ = '\0';
	return mem;
}

//
//
//
//

static inline void sb_regress(Stringer *sb, int num) {
	sb->min -= num;
	sb->buf[sb->min] = '\0';
}

//
//
//
//

static inline char *sb_writetextl(Stringer *sb, const char *text, int l) {
	char *mem = sb_alloc(sb, l + 1, l);
	memcpy(mem, text, l);
	mem[l] = 0;
	return mem;
}

//
//
//
//

// todo: perf!
static inline char *sb_writetextesc(Stringer *sb, const char *str) {
	char *mem = sb->buf + sb->min;
	while (*str) {
		switch (*str) {
			case '\\': {
				str ++;
				sb_writechar(sb, '\\');
				sb_writechar(sb, '\\');
			} break;
			default: {
				sb_writechar(sb, *str ++);
			} break;
		}
	}
	return mem;
}

//
//
//
//

static inline char *sb_writetext(Stringer *sb, const char *str) {
	char *mem = sb->buf + sb->min;

	// todo: perf
	while (*str) {
		sb_writechar(sb, *str ++);
	}
	return mem;
}

//
//
//
//

static void sb_repeatchar(Stringer *sb, int chr, int num) {
	char *mem = sb_alloc(sb, num + 1, num);
	memset(mem, chr, num);
	mem[num] = '\0';
}

//
//
//
//

static int sb_writetextf(Stringer *sb, char *format, ...) {
	va_list vargs;
	va_start(vargs, format);
	int size = stbsp_vsnprintf(NULL, 0, format, vargs);
	char *text = sb_alloc(sb, size + 1, size);
	stbsp_vsnprintf(text, size + 1, format, vargs);
	va_end(vargs);
	return size;
}

//
//
//
//

#endif



