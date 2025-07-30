/*
** See Copyright Notice In elf.h
** log.c
** Simple Logging Tools
*/

enum {
	LOG_KDEBUG = 0, LOG_KINFO, LOG_KWARNING, LOG_KERROR, LOG_KFATAL
};

#define elf_log(TYPE,FORMAT,...) (CHECK_FORMAT(FORMAT,##__VA_ARGS__),elf_log_(TYPE,DBG_SOURCE,FORMAT,##__VA_ARGS__))

#define elf_info_log(yyy,...)    elf_log(LOG_KINFO,yyy,##__VA_ARGS__)
#define elf_debug_log(yyy,...)   elf_log(LOG_KDEBUG,yyy,##__VA_ARGS__)
#define elf_warning_log(yyy,...) elf_log(LOG_KWARNING,yyy,##__VA_ARGS__)
#define elf_error_log(yyy,...)   elf_log(LOG_KERROR,yyy,##__VA_ARGS__)
#define elf_fatal_log(yyy,...)   elf_log(LOG_KFATAL,yyy,##__VA_ARGS__)


static char *S_filename(char *s) {
	char *r;

	for(r = s; *s != 0; s += 1) {
		if (*s=='/' || *s=='\\') {
			r=s+1;
		}
	}
	return r;
}


static char *log2s(int type) {
	switch (type) {
		case LOG_KDEBUG: return "DEBUG";
		case LOG_KINFO: return "INFO";
		case LOG_KWARNING: return "WARN";
		case LOG_KERROR: return "ERROR";
		case LOG_KFATAL: return "FATAL";
		default: return "OTHER";
	}
}


static void elf_log_(int type, Debug_Source source, const char *fmt, ...) {

	char b[0x1000];

	va_list xx;
	va_start(xx,fmt);

	stbsp_vsnprintf(b,sizeof(b),fmt,xx);

	va_end(xx);

	int line = source.lineNumber;
	char *file = (char*) source.fileName;
	char *func = (char*) source.func;

	printf("%s %s[%i] %s(): %s\n",log2s(type),S_filename(file),line,func,b);
}



