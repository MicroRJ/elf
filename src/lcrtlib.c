// /*
// ** See Copyright Notice In elf.h
// ** lcrtlib.c
// ** CRT Library
// */


// #include <time.h>

// #if defined(_WIN32)
// #include <conio.h>
// #include <process.h>
// #include <io.h>
// #include <direct.h>
// #endif


// int crtlib__chdir(elState *rt) {
// 	elString *name = elf_get_string(rt,0);
// #if defined(PLATFORM_WEB)
// 	elf_put_integer(rt,chdir(name->c));
// #else
// 	elf_put_integer(rt,_chdir(name->c));
// #endif
// 	return 1;
// }


// #if defined(_MSC_VER)

// int crtlib__getch(elState *rt) {
// 	elf_put_integer(rt,_getch());
// 	return 1;
// }


// int crtlib__getpid(elState *rt) {
// 	elf_put_integer(rt,_getpid());
// 	return 1;
// }


// int crtlib_time(elState *rt) {
// 	elf_put_integer(rt,time(0));
// 	return 1;
// }


// int crtlib_clock(elState *rt) {
// 	elf_put_integer(rt,clock());
// 	return 1;
// }


// int crtlib__strdate(elState *rt) {
// 	char buf[128];
// 	_strdate_s(buf,sizeof(buf));
// 	elf_put_new_string(rt,buf);
// 	return 1;
// }


// int crtlib__strtime(elState *rt) {
// 	char buf[128];
// 	_strtime_s(buf,sizeof(buf));
// 	elf_put_new_string(rt,buf);
// 	return 1;
// }


// int crtlib__unlink(elState *rt) {
// 	elString *name = elf_get_string(rt,0);
// 	elf_put_integer(rt,_unlink(name->c));
// 	return 1;
// }


// int crtlib__unlock_file(elState *rt) {
// 	elHandle file = elf_get_handle(rt,0);
// 	_unlock_file(file);
// 	return 0;
// }


// int crtlib__write(elState *rt) {
// 	elHandle file = elf_get_handle(rt,0);
// 	elString *buf = elf_get_string(rt,1);
// 	elf_put_integer(rt,_write((elInteger)file,buf->c,buf->length));
// 	return 1;
// }


// int crtlib__close(elState *rt) {
// 	elHandle file = elf_get_handle(rt,0);
// 	elf_put_integer(rt,_close((int)(elInteger)file));
// 	return 1;
// }


// int crtlib__commit(elState *rt) {
// 	elHandle file = elf_get_handle(rt,0);
// 	elf_put_integer(rt,_commit((int)(elInteger)file));
// 	return 1;
// }


// int crtlib__chdrive(elState *rt) {
// 	elInteger letter = elf_get_integer(rt,0);
// 	elf_put_integer(rt,_chdrive(letter));
// 	return 1;
// }


// int crtlib__chmode(elState *rt) {
// 	elString *name = elf_get_string(rt,0);
// 	elInteger mode = elf_get_integer(rt,1);
// 	elf_put_integer(rt,_chmod(name->c,mode));
// 	return 1;
// }


// int crtlib__execl(elState *rt) {
// 	elString *cl = elf_get_string(rt,0);
// 	elf_put_integer(rt,_execl(cl->c,0,0));
// 	return 1;
// }


// int crtlib_system(elState *rt) {
// 	elString *cl = elf_get_string(rt,0);
// 	elf_put_integer(rt,system(cl->c));
// 	return 1;
// }
// #else

// #define DEFSTUB(NAME) \
// int NAME(elState *R) {\
// 	elf_error_log(TO_TEXT(NAME)"(): not implemented for this platform");\
// 	return 0;\
// }

// DEFSTUB(crtlib__getch)
// DEFSTUB(crtlib__getpid)
// DEFSTUB(crtlib_time)
// DEFSTUB(crtlib_clock)
// DEFSTUB(crtlib__strdate)
// DEFSTUB(crtlib__strtime)
// DEFSTUB(crtlib__unlink)
// DEFSTUB(crtlib__unlock_file)
// DEFSTUB(crtlib__write)
// DEFSTUB(crtlib__close)
// DEFSTUB(crtlib__commit)
// DEFSTUB(crtlib__chdrive)
// DEFSTUB(crtlib__chmode)
// DEFSTUB(crtlib__execl)
// DEFSTUB(crtlib_system)
// #endif

// elAPI void crtlib_load(elState *rt) {
// 	elModule *md = rt->md;



// 	elf_gset(md,elf_put_new_string(rt,"_execl"),elCFN(crtlib__execl));
// 	elf_gset(md,elf_put_new_string(rt,"system"),elCFN(crtlib_system));
// 	elf_gset(md,elf_put_new_string(rt,"_getch"),elCFN(crtlib__getch));
// 	elf_gset(md,elf_put_new_string(rt,"time"),elCFN(crtlib_time));
// 	elf_gset(md,elf_put_new_string(rt,"_getpid"),elCFN(crtlib__getpid));
// 	elf_gset(md,elf_put_new_string(rt,"_strdate"),elCFN(crtlib__strdate));
// 	elf_gset(md,elf_put_new_string(rt,"_strtime"),elCFN(crtlib__strtime));

// 	elf_gset(md,elf_put_new_string(rt,"_unlink"),elCFN(crtlib__unlink));
// 	elf_gset(md,elf_put_new_string(rt,"_unlock_file"),elCFN(crtlib__unlock_file));
// 	elf_gset(md,elf_put_new_string(rt,"_write"),elCFN(crtlib__write));
// 	elf_gset(md,elf_put_new_string(rt,"_commit"),elCFN(crtlib__commit));
// 	elf_gset(md,elf_put_new_string(rt,"_close"),elCFN(crtlib__close));
// 	elf_gset(md,elf_put_new_string(rt,"_chdir"),elCFN(crtlib__chdir));
// 	elf_gset(md,elf_put_new_string(rt,"_chdrive"),elCFN(crtlib__chdrive));
// 	elf_gset(md,elf_put_new_string(rt,"clock"),elCFN(crtlib_clock));
// }