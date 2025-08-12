//
// See Copyright Notice In elf.h
//

#ifndef STRING_BUILDER_H
#define STRING_BUILDER_H


#include <malloc.h>


typedef struct {
	int   max;
	int   min;
	char *buf;
} String_Builder;


static char *sb_alloc(String_Builder *sb, int res, int com) {
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


static inline char *sb_writechar(String_Builder *sb, int chr) {
	char *mem = sb->buf + sb->min;

	char *text = sb_alloc(sb, 1 + 1, 1);

	*text ++ = chr;
	*text ++ = '\0';
	return mem;
}


static inline char *sb_writestr(String_Builder *sb, const char *str) {
	char *mem = sb->buf + sb->min;

	// todo: perf
	while (*str) {
		sb_writechar(sb, *str ++);
	}
	return mem;
}



#endif



