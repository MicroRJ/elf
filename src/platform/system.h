//
// See Copyright Notice In elf.h
//

#ifndef SYSTEM_H
#define SYSTEM_H

//
// Several Functions And Structs Used To
// Interface With The Operating System.
//

// TODO: TOMORROW!
int sys_file_size(int file);
int sys_file_open(char *name);
int sys_file_close(int file);
int sys_file_read(int file, char *buf, int size);
int sys_file_write(int file, char *buf, int size);


void *sys_virtual_alloc(elf_i64 length);


/* printing, use type (LOG_TYPE),
for instance, for web,
LOG = console.log, ERROR = console.err */
void sys_console_print(int type, char *message);


/* triggers the debugger for this program,
returns whether a debugger was successfully
attached */
bool sys_debugger();


/* this uses milliseconds, but seconds would
by my preference, since it is an SI unit */
void sys_sleep(elf_i64 ms);


/* the clock frequency, use to translate
clock time to seconds, for web you might get
milliseconds, so freq=1000, for desktop,
you get a performance counter, which has
a nano-second resolution  */
elf_i64 sys_get_clock_freq();

/* get the highest resolution clock available */
elf_i64 sys_get_clock_time();


int sys_get_my_name(int length, char *text);
int sys_get_my_pid();

int sys_get_work_dir(int length, char *text);
int sys_set_work_dir(char *text);


elf_Handle sys_load_dll(char const *name);
void *sys_get_dll_fn(elf_Handle lib, char const *name);


int sys_get_last_error();
void sys_get_error_msg(int error, char *buff, int len);


enum {
	FILE_TYPE_FILE    = 0,
	FILE_TYPE_FOLDER  = 1,
	FILE_TYPE_SYMLINK = 2,
};


//	typedef struct FileEntry {
//		char name[256];
//		int  tags;
//		int  size;
//	} FileEntry;
//
//	FileEntry *sys_list_folder(const char *name, int *nentries);


// todo: make better!
typedef struct {
	void *hand;
	int   type;
	int   size;
	char  name[1024];
} FILE_VISITOR;

int sys_opendir(FILE_VISITOR *visitor, char *const path);
int sys_readdir(FILE_VISITOR *visitor);
void sys_closedir(FILE_VISITOR *visitor);


#endif