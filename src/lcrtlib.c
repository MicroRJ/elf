/*
** See Copyright Notice In elf.h
** lcrtlib.c
** CRT Library
*/


#include <time.h>

#if defined(_WIN32)
#include <conio.h>
#include <process.h>
#include <io.h>
#include <direct.h>
#endif


int crtlib__chdir(elState *rt) {
	elString *name = elf_getstr(rt,0);
#if defined(PLATFORM_WEB)
	elf_push_integer(rt,chdir(name->c));
#else
	elf_push_integer(rt,_chdir(name->c));
#endif
	return 1;
}


#if defined(_MSC_VER)

int crtlib__getch(elState *rt) {
	elf_push_integer(rt,_getch());
	return 1;
}


int crtlib__getpid(elState *rt) {
	elf_push_integer(rt,_getpid());
	return 1;
}


int crtlib_time(elState *rt) {
	elf_push_integer(rt,time(0));
	return 1;
}


int crtlib_clock(elState *rt) {
	elf_push_integer(rt,clock());
	return 1;
}


int crtlib__strdate(elState *rt) {
	char buf[128];
	_strdate_s(buf,sizeof(buf));
	elf_push_new_string(rt,buf);
	return 1;
}


int crtlib__strtime(elState *rt) {
	char buf[128];
	_strtime_s(buf,sizeof(buf));
	elf_push_new_string(rt,buf);
	return 1;
}


int crtlib__unlink(elState *rt) {
	elString *name = elf_getstr(rt,0);
	elf_push_integer(rt,_unlink(name->c));
	return 1;
}


int crtlib__unlock_file(elState *rt) {
	elHandle file = elf_getsys(rt,0);
	_unlock_file(file);
	return 0;
}


int crtlib__write(elState *rt) {
	elHandle file = elf_getsys(rt,0);
	elString *buf = elf_getstr(rt,1);
	elf_push_integer(rt,_write((elInteger)file,buf->c,buf->length));
	return 1;
}


int crtlib__close(elState *rt) {
	elHandle file = elf_getsys(rt,0);
	elf_push_integer(rt,_close((int)(elInteger)file));
	return 1;
}


int crtlib__commit(elState *rt) {
	elHandle file = elf_getsys(rt,0);
	elf_push_integer(rt,_commit((int)(elInteger)file));
	return 1;
}


int crtlib__chdrive(elState *rt) {
	elInteger letter = elf_getint(rt,0);
	elf_push_integer(rt,_chdrive(letter));
	return 1;
}


int crtlib__chmode(elState *rt) {
	elString *name = elf_getstr(rt,0);
	elInteger mode = elf_getint(rt,1);
	elf_push_integer(rt,_chmod(name->c,mode));
	return 1;
}


int crtlib__execl(elState *rt) {
	elString *cl = elf_getstr(rt,0);
	elf_push_integer(rt,_execl(cl->c,0,0));
	return 1;
}


int crtlib_system(elState *rt) {
	elString *cl = elf_getstr(rt,0);
	elf_push_integer(rt,system(cl->c));
	return 1;
}
#else

#define DEFSTUB(NAME) \
int NAME(elState *R) {\
	elf_logerror(XSTRINGIFY(NAME)"(): not implemented for this platform");\
	return 0;\
}

DEFSTUB(crtlib__getch)
DEFSTUB(crtlib__getpid)
DEFSTUB(crtlib_time)
DEFSTUB(crtlib_clock)
DEFSTUB(crtlib__strdate)
DEFSTUB(crtlib__strtime)
DEFSTUB(crtlib__unlink)
DEFSTUB(crtlib__unlock_file)
DEFSTUB(crtlib__write)
DEFSTUB(crtlib__close)
DEFSTUB(crtlib__commit)
DEFSTUB(crtlib__chdrive)
DEFSTUB(crtlib__chmode)
DEFSTUB(crtlib__execl)
DEFSTUB(crtlib_system)
#endif

elf_api void crtlib_load(elState *rt) {
	elModule *md = rt->md;



	elf_add_global_value(md,elf_push_new_string(rt,"_execl"),elf_valbid(crtlib__execl));
	elf_add_global_value(md,elf_push_new_string(rt,"system"),elf_valbid(crtlib_system));
	elf_add_global_value(md,elf_push_new_string(rt,"_getch"),elf_valbid(crtlib__getch));
	elf_add_global_value(md,elf_push_new_string(rt,"time"),elf_valbid(crtlib_time));
	elf_add_global_value(md,elf_push_new_string(rt,"_getpid"),elf_valbid(crtlib__getpid));
	elf_add_global_value(md,elf_push_new_string(rt,"_strdate"),elf_valbid(crtlib__strdate));
	elf_add_global_value(md,elf_push_new_string(rt,"_strtime"),elf_valbid(crtlib__strtime));

	elf_add_global_value(md,elf_push_new_string(rt,"_unlink"),elf_valbid(crtlib__unlink));
	elf_add_global_value(md,elf_push_new_string(rt,"_unlock_file"),elf_valbid(crtlib__unlock_file));
	elf_add_global_value(md,elf_push_new_string(rt,"_write"),elf_valbid(crtlib__write));
	elf_add_global_value(md,elf_push_new_string(rt,"_commit"),elf_valbid(crtlib__commit));
	elf_add_global_value(md,elf_push_new_string(rt,"_close"),elf_valbid(crtlib__close));
	elf_add_global_value(md,elf_push_new_string(rt,"_chdir"),elf_valbid(crtlib__chdir));
	elf_add_global_value(md,elf_push_new_string(rt,"_chdrive"),elf_valbid(crtlib__chdrive));
	elf_add_global_value(md,elf_push_new_string(rt,"clock"),elf_valbid(crtlib_clock));
}