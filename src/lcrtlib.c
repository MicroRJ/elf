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


int crtlib__chdir(elf_State *rt) {
	elf_String *name = elf_getstr(rt,0);
#if defined(PLATFORM_WEB)
	elf_pushint(rt,chdir(name->c));
#else
	elf_pushint(rt,_chdir(name->c));
#endif
	return 1;
}


#if defined(_MSC_VER)

int crtlib__getch(elf_State *rt) {
	elf_pushint(rt,_getch());
	return 1;
}


int crtlib__getpid(elf_State *rt) {
	elf_pushint(rt,_getpid());
	return 1;
}


int crtlib_time(elf_State *rt) {
	elf_pushint(rt,time(0));
	return 1;
}


int crtlib_clock(elf_State *rt) {
	elf_pushint(rt,clock());
	return 1;
}


int crtlib__strdate(elf_State *rt) {
	char buf[128];
	_strdate_s(buf,sizeof(buf));
	elf_pushnewstr(rt,buf);
	return 1;
}


int crtlib__strtime(elf_State *rt) {
	char buf[128];
	_strtime_s(buf,sizeof(buf));
	elf_pushnewstr(rt,buf);
	return 1;
}


int crtlib__unlink(elf_State *rt) {
	elf_String *name = elf_getstr(rt,0);
	elf_pushint(rt,_unlink(name->c));
	return 1;
}


int crtlib__unlock_file(elf_State *rt) {
	elf_Handle file = elf_getsys(rt,0);
	_unlock_file(file);
	return 0;
}


int crtlib__write(elf_State *rt) {
	elf_Handle file = elf_getsys(rt,0);
	elf_String *buf = elf_getstr(rt,1);
	elf_pushint(rt,_write((elf_int)file,buf->c,buf->length));
	return 1;
}


int crtlib__close(elf_State *rt) {
	elf_Handle file = elf_getsys(rt,0);
	elf_pushint(rt,_close((int)(elf_int)file));
	return 1;
}


int crtlib__commit(elf_State *rt) {
	elf_Handle file = elf_getsys(rt,0);
	elf_pushint(rt,_commit((int)(elf_int)file));
	return 1;
}


int crtlib__chdrive(elf_State *rt) {
	elf_int letter = elf_getint(rt,0);
	elf_pushint(rt,_chdrive(letter));
	return 1;
}


int crtlib__chmode(elf_State *rt) {
	elf_String *name = elf_getstr(rt,0);
	elf_int mode = elf_getint(rt,1);
	elf_pushint(rt,_chmod(name->c,mode));
	return 1;
}


int crtlib__execl(elf_State *rt) {
	elf_String *cl = elf_getstr(rt,0);
	elf_pushint(rt,_execl(cl->c,0,0));
	return 1;
}


int crtlib_system(elf_State *rt) {
	elf_String *cl = elf_getstr(rt,0);
	elf_pushint(rt,system(cl->c));
	return 1;
}
#else

#define DEFSTUB(NAME) \
int NAME(elf_State *R) {\
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

elf_api void crtlib_load(elf_State *rt) {
	elf_Module *md = rt->md;



	lang_addglobal(md,elf_pushnewstr(rt,"_execl"),elf_valbid(crtlib__execl));
	lang_addglobal(md,elf_pushnewstr(rt,"system"),elf_valbid(crtlib_system));
	lang_addglobal(md,elf_pushnewstr(rt,"_getch"),elf_valbid(crtlib__getch));
	lang_addglobal(md,elf_pushnewstr(rt,"time"),elf_valbid(crtlib_time));
	lang_addglobal(md,elf_pushnewstr(rt,"_getpid"),elf_valbid(crtlib__getpid));
	lang_addglobal(md,elf_pushnewstr(rt,"_strdate"),elf_valbid(crtlib__strdate));
	lang_addglobal(md,elf_pushnewstr(rt,"_strtime"),elf_valbid(crtlib__strtime));

	lang_addglobal(md,elf_pushnewstr(rt,"_unlink"),elf_valbid(crtlib__unlink));
	lang_addglobal(md,elf_pushnewstr(rt,"_unlock_file"),elf_valbid(crtlib__unlock_file));
	lang_addglobal(md,elf_pushnewstr(rt,"_write"),elf_valbid(crtlib__write));
	lang_addglobal(md,elf_pushnewstr(rt,"_commit"),elf_valbid(crtlib__commit));
	lang_addglobal(md,elf_pushnewstr(rt,"_close"),elf_valbid(crtlib__close));
	lang_addglobal(md,elf_pushnewstr(rt,"_chdir"),elf_valbid(crtlib__chdir));
	lang_addglobal(md,elf_pushnewstr(rt,"_chdrive"),elf_valbid(crtlib__chdrive));
	lang_addglobal(md,elf_pushnewstr(rt,"clock"),elf_valbid(crtlib_clock));
}