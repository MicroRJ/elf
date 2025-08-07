//
// See Copyright Notice In elf.h
//

enum {
	LOG_KDEBUG = 0, LOG_KINFO, LOG_KWARNING, LOG_KERROR, LOG_KFATAL
};

#define elf_log(TYPE,FORMAT,...) (CHECK_FORMAT(FORMAT,##__VA_ARGS__), elf_log_(TYPE,__FILE__,__func__,__LINE__, FORMAT, ##__VA_ARGS__))

#define elf_info_log(yyy,...)    elf_log(LOG_KINFO,yyy,##__VA_ARGS__)
#define elf_debug_log(yyy,...)   elf_log(LOG_KDEBUG,yyy,##__VA_ARGS__)
#define elf_warning_log(yyy,...) elf_log(LOG_KWARNING,yyy,##__VA_ARGS__)
#define elf_lerror(yyy,...)   elf_log(LOG_KERROR,yyy,##__VA_ARGS__)
#define elf_fatal_log(yyy,...)   elf_log(LOG_KFATAL,yyy,##__VA_ARGS__)




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
static void elf_log_(int type, const char *file, const char *func, int line, const char *fmt, ...) {
	// todo: thread alloc instead!
	char b[1024];

	va_list va;
	va_start(va, fmt);
	stbsp_vsnprintf(b,sizeof(b),fmt,va);
	va_end(va);

	char *name;

	name=get_name_from_file_path(file);
	printf("%s %s[%i] %s(): %s\n",log2s(type),name,line,func,b);
}



