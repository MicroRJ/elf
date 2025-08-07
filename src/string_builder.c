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


static char *string_builder_alloc(String_Builder *sb, int res, int com) {
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


static inline int bwritechar(String_Builder *sb, int chr) {
	char *text = string_builder_alloc(sb, 1 + 1, 1);
	*text ++ = chr;
	*text ++ = '\0';
	return 1;
}



#endif



