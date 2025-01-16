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


// int crtlib__chdir(elf_State *rt) {
// 	elf_String *name = elf_get_string(rt,0);
// #if defined(PLATFORM_WEB)
// 	elf_push_integer(rt,chdir(name->c));
// #else
// 	elf_push_integer(rt,_chdir(name->c));
// #endif
// 	return 1;
// }


// #if defined(_MSC_VER)

// int crtlib__getch(elf_State *rt) {
// 	elf_push_integer(rt,_getch());
// 	return 1;
// }


// int crtlib__getpid(elf_State *rt) {
// 	elf_push_integer(rt,_getpid());
// 	return 1;
// }


// int crtlib_time(elf_State *rt) {
// 	elf_push_integer(rt,time(0));
// 	return 1;
// }


// int crtlib_clock(elf_State *rt) {
// 	elf_push_integer(rt,clock());
// 	return 1;
// }


// int crtlib__strdate(elf_State *rt) {
// 	char buf[128];
// 	_strdate_s(buf,sizeof(buf));
// 	elf_new_string(rt,buf);
// 	return 1;
// }


// int crtlib__strtime(elf_State *rt) {
// 	char buf[128];
// 	_strtime_s(buf,sizeof(buf));
// 	elf_new_string(rt,buf);
// 	return 1;
// }


// int crtlib__unlink(elf_State *rt) {
// 	elf_String *name = elf_get_string(rt,0);
// 	elf_push_integer(rt,_unlink(name->c));
// 	return 1;
// }


// int crtlib__unlock_file(elf_State *rt) {
// 	elf_Handle file = elf_get_sysobj(rt,0);
// 	_unlock_file(file);
// 	return 0;
// }


// int crtlib__write(elf_State *rt) {
// 	elf_Handle file = elf_get_sysobj(rt,0);
// 	elf_String *buf = elf_get_string(rt,1);
// 	elf_push_integer(rt,_write((elf_Int)file,buf->c,buf->length));
// 	return 1;
// }


// int crtlib__close(elf_State *rt) {
// 	elf_Handle file = elf_get_sysobj(rt,0);
// 	elf_push_integer(rt,_close((int)(elf_Int)file));
// 	return 1;
// }


// int crtlib__commit(elf_State *rt) {
// 	elf_Handle file = elf_get_sysobj(rt,0);
// 	elf_push_integer(rt,_commit((int)(elf_Int)file));
// 	return 1;
// }


// int crtlib__chdrive(elf_State *rt) {
// 	elf_Int letter = elf_get_int(rt,0);
// 	elf_push_integer(rt,_chdrive(letter));
// 	return 1;
// }


// int crtlib__chmode(elf_State *rt) {
// 	elf_String *name = elf_get_string(rt,0);
// 	elf_Int mode = elf_get_int(rt,1);
// 	elf_push_integer(rt,_chmod(name->c,mode));
// 	return 1;
// }


// int crtlib__execl(elf_State *rt) {
// 	elf_String *cl = elf_get_string(rt,0);
// 	elf_push_integer(rt,_execl(cl->c,0,0));
// 	return 1;
// }


// int crtlib_system(elf_State *rt) {
// 	elf_String *cl = elf_get_string(rt,0);
// 	elf_push_integer(rt,system(cl->c));
// 	return 1;
// }
// #else

// #define DEFSTUB(NAME) \
// int NAME(elf_State *R) {\
// 	elf_error_log(XTEXT(NAME)"(): not implemented for this platform");\
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

// elAPI void crtlib_load(elf_State *rt) {
// 	elf_Module *md = rt->md;



// 	elf_set_global(md,elf_new_string(rt,"_execl"),VCFN(crtlib__execl));
// 	elf_set_global(md,elf_new_string(rt,"system"),VCFN(crtlib_system));
// 	elf_set_global(md,elf_new_string(rt,"_getch"),VCFN(crtlib__getch));
// 	elf_set_global(md,elf_new_string(rt,"time"),VCFN(crtlib_time));
// 	elf_set_global(md,elf_new_string(rt,"_getpid"),VCFN(crtlib__getpid));
// 	elf_set_global(md,elf_new_string(rt,"_strdate"),VCFN(crtlib__strdate));
// 	elf_set_global(md,elf_new_string(rt,"_strtime"),VCFN(crtlib__strtime));

// 	elf_set_global(md,elf_new_string(rt,"_unlink"),VCFN(crtlib__unlink));
// 	elf_set_global(md,elf_new_string(rt,"_unlock_file"),VCFN(crtlib__unlock_file));
// 	elf_set_global(md,elf_new_string(rt,"_write"),VCFN(crtlib__write));
// 	elf_set_global(md,elf_new_string(rt,"_commit"),VCFN(crtlib__commit));
// 	elf_set_global(md,elf_new_string(rt,"_close"),VCFN(crtlib__close));
// 	elf_set_global(md,elf_new_string(rt,"_chdir"),VCFN(crtlib__chdir));
// 	elf_set_global(md,elf_new_string(rt,"_chdrive"),VCFN(crtlib__chdrive));
// 	elf_set_global(md,elf_new_string(rt,"clock"),VCFN(crtlib_clock));
// }