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


// int crtlib__chdir(elf_Shell *rt) {
// 	elf_String *name = elf_get_str(rt,0);
// #if defined(PLATFORM_WEB)
// 	elf_add_int(rt,chdir(name->c));
// #else
// 	elf_add_int(rt,_chdir(name->c));
// #endif
// 	return 1;
// }


// #if defined(_MSC_VER)

// int crtlib__getch(elf_Shell *rt) {
// 	elf_add_int(rt,_getch());
// 	return 1;
// }


// int crtlib__getpid(elf_Shell *rt) {
// 	elf_add_int(rt,_getpid());
// 	return 1;
// }


// int crtlib_time(elf_Shell *rt) {
// 	elf_add_int(rt,time(0));
// 	return 1;
// }


// int crtlib_clock(elf_Shell *rt) {
// 	elf_add_int(rt,clock());
// 	return 1;
// }


// int crtlib__strdate(elf_Shell *rt) {
// 	char buf[128];
// 	_strdate_s(buf,sizeof(buf));
// 	elf_new_string(rt,buf);
// 	return 1;
// }


// int crtlib__strtime(elf_Shell *rt) {
// 	char buf[128];
// 	_strtime_s(buf,sizeof(buf));
// 	elf_new_string(rt,buf);
// 	return 1;
// }


// int crtlib__unlink(elf_Shell *rt) {
// 	elf_String *name = elf_get_str(rt,0);
// 	elf_add_int(rt,_unlink(name->c));
// 	return 1;
// }


// int crtlib__unlock_file(elf_Shell *rt) {
// 	elf_Handle file = elf_get_sys(rt,0);
// 	_unlock_file(file);
// 	return 0;
// }


// int crtlib__write(elf_Shell *rt) {
// 	elf_Handle file = elf_get_sys(rt,0);
// 	elf_String *buf = elf_get_str(rt,1);
// 	elf_add_int(rt,_write((elf_Int)file,buf->c,buf->length));
// 	return 1;
// }


// int crtlib__close(elf_Shell *rt) {
// 	elf_Handle file = elf_get_sys(rt,0);
// 	elf_add_int(rt,_close((int)(elf_Int)file));
// 	return 1;
// }


// int crtlib__commit(elf_Shell *rt) {
// 	elf_Handle file = elf_get_sys(rt,0);
// 	elf_add_int(rt,_commit((int)(elf_Int)file));
// 	return 1;
// }


// int crtlib__chdrive(elf_Shell *rt) {
// 	elf_Int letter = elf_get_int(rt,0);
// 	elf_add_int(rt,_chdrive(letter));
// 	return 1;
// }


// int crtlib__chmode(elf_Shell *rt) {
// 	elf_String *name = elf_get_str(rt,0);
// 	elf_Int mode = elf_get_int(rt,1);
// 	elf_add_int(rt,_chmod(name->c,mode));
// 	return 1;
// }


// int crtlib__execl(elf_Shell *rt) {
// 	elf_String *cl = elf_get_str(rt,0);
// 	elf_add_int(rt,_execl(cl->c,0,0));
// 	return 1;
// }


// int crtlib_system(elf_Shell *rt) {
// 	elf_String *cl = elf_get_str(rt,0);
// 	elf_add_int(rt,system(cl->c));
// 	return 1;
// }
// #else

// #define DEFSTUB(NAME) \
// int NAME(elf_Shell *R) {\
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

// elAPI void crtlib_load(elf_Shell *rt) {
// 	elf_Module *md = rt->md;



// 	elf_gset(md,elf_new_string(rt,"_execl"),VCFN(crtlib__execl));
// 	elf_gset(md,elf_new_string(rt,"system"),VCFN(crtlib_system));
// 	elf_gset(md,elf_new_string(rt,"_getch"),VCFN(crtlib__getch));
// 	elf_gset(md,elf_new_string(rt,"time"),VCFN(crtlib_time));
// 	elf_gset(md,elf_new_string(rt,"_getpid"),VCFN(crtlib__getpid));
// 	elf_gset(md,elf_new_string(rt,"_strdate"),VCFN(crtlib__strdate));
// 	elf_gset(md,elf_new_string(rt,"_strtime"),VCFN(crtlib__strtime));

// 	elf_gset(md,elf_new_string(rt,"_unlink"),VCFN(crtlib__unlink));
// 	elf_gset(md,elf_new_string(rt,"_unlock_file"),VCFN(crtlib__unlock_file));
// 	elf_gset(md,elf_new_string(rt,"_write"),VCFN(crtlib__write));
// 	elf_gset(md,elf_new_string(rt,"_commit"),VCFN(crtlib__commit));
// 	elf_gset(md,elf_new_string(rt,"_close"),VCFN(crtlib__close));
// 	elf_gset(md,elf_new_string(rt,"_chdir"),VCFN(crtlib__chdir));
// 	elf_gset(md,elf_new_string(rt,"_chdrive"),VCFN(crtlib__chdrive));
// 	elf_gset(md,elf_new_string(rt,"clock"),VCFN(crtlib_clock));
// }