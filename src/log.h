/*
** See Copyright Notice In elf.h
** log.h
*/

enum {
	LOG_KDEBUG = 0, LOG_KINFO, LOG_KWARNING, LOG_KERROR, LOG_KFATAL
};

char *log2s(int);


typedef void (elf_logger)(int type, Debug_Source source, char const *fmt, ...);
elAPI void elf_set_logger(int type, Debug_Source loc, char const *fmt, ...);

elAPI void elf_log_(int type, Debug_Source loc, char const *fmt, ...);


#define elf_log(TYPE,FORMAT,...) (CHECK_FORMAT(FORMAT,##__VA_ARGS__),elf_log_(TYPE,DBG_SOURCE,FORMAT,##__VA_ARGS__))
#define elf_info_log(yyy,...)    elf_log(LOG_KINFO,yyy,##__VA_ARGS__)
#define elf_debug_log(yyy,...)   elf_log(LOG_KDEBUG,yyy,##__VA_ARGS__)
#define elf_warning_log(yyy,...) elf_log(LOG_KWARNING,yyy,##__VA_ARGS__)
#define elf_error_log(yyy,...)   elf_log(LOG_KERROR,yyy,##__VA_ARGS__)
#define elf_fatal_log(yyy,...)   elf_log(LOG_KFATAL,yyy,##__VA_ARGS__)
