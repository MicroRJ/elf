//
// See Copyright Notice In elf.h
//

#ifndef SYSTEM_H
#define SYSTEM_H

//
// Several Functions And Structs Used To
// Interface With The Operating System.
//


#if !defined(FILE_HANDLE)
#define FILE_HANDLE _FILE_HANDLE
#endif

#if !defined(FILE_TIME)
#define FILE_TIME _FILE_TIME
#endif

#if !defined(SYSTEM_TIME)
#define SYSTEM_TIME _SYSTEM_TIME
#endif


typedef elf_Handle _FILE_HANDLE;

// must match windows headers, for now
typedef struct {
	elf_u16 year;
	elf_u16 month;
	elf_u16 dayofweek;
	elf_u16 day;
	elf_u16 hour;
	elf_u16 minute;
	elf_u16 second;
	elf_u16 milliseconds;
} _SYSTEM_TIME;

// must match windows headers, for now
typedef union {
	struct { elf_u32 low, high; };
	elf_u64 time;
} _FILE_TIME;

typedef struct {
	FILE_TIME create;
	FILE_TIME write;
	// read / write or ran
	FILE_TIME access;
} FILE_TIMES;

enum {
	// create a new file or truncate the existing one
	SYS_CREATE_ALWAYS,
	// create a new file, fail if it already exists
	SYS_CREATE_NEW,
	// truncate (zero) a file if it exists, otherwise fail
	SYS_TRUNCATE_EXISTING,

	// open an existing file, or create a new one
	SYS_OPEN_ALWAYS,
	// open an existing file, otherwise fail
	SYS_OPEN_EXISTING,
};

enum {
	SYS_OPEN_READ     = 1,
	SYS_OPEN_WRITE    = 2,
	SYS_OPEN_EXECUTE  = 4,
	SYS_SHARE_READ    = 8,
	SYS_SHARE_WRITE   = 16,
};


int sys_create_directory(char *path);

bool sys_delete_file(char *name);
FILE_HANDLE sys_open_file(char *name, int flags, int options);
void sys_close_file(FILE_HANDLE file);
unsigned int sys_size_file(FILE_HANDLE file);
unsigned int sys_read_file(FILE_HANDLE file, char *buf, unsigned int from, unsigned int size);
unsigned int sys_write_file(FILE_HANDLE file, char *buf, unsigned int from, unsigned int size);
int sys_time_file(FILE_HANDLE file, FILE_TIMES *);

void sys_file_time_to_system_time(FILE_TIME *, SYSTEM_TIME *);

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


// int sys_get_my_name(int length, char *text);
int sys_get_this_process_id();
void sys_exit_this_process(int errorcode);

int sys_get_work_dir(char *buf, int bufsize);
int sys_set_work_dir(char *buf);


elf_Handle sys_load_dll(char const *name);
void *sys_get_dll_fn(elf_Handle lib, char const *name);


int sys_get_last_error();
void sys_get_error_msg(int error, char *buff, int len);

elf_Handle sys_create_process(char const *file, char const *args);

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

int sys_open_directory(FILE_VISITOR *visitor, char *const path);
int sys_read_directory(FILE_VISITOR *visitor);
void sys_closedir(FILE_VISITOR *visitor);


#endif