//
// See Copyright Notice In elf.h
//

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

static int bprintf(String_Builder *sb, char *format, ...) {
	va_list vargs;
	va_start(vargs, format);
	int length = stbsp_vsnprintf(NULL, 0, format, vargs);
	char *text = string_builder_alloc(sb, length + 1, length);
	stbsp_vsnprintf(text, length + 1, format, vargs);
	va_end(vargs);
	return length;
}



