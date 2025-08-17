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
	SYS_CURRENT = 0,
	SYS_BEGIN,
	SYS_END,
};

enum {
	SYS_OPEN_READ     = 1,
	SYS_OPEN_WRITE    = 2,
	SYS_OPEN_EXECUTE  = 4,
	SYS_SHARE_READ    = 8,
	SYS_SHARE_WRITE   = 16,
	// write must be a multiple of the sector size
	SYS_NO_BUFFERING  = 32,
};

enum {
	SYS_STD_OUTPUT = 0,
	SYS_STD_ERROR,
	SYS_STD_INPUT,
};

// i hear that on linux you can't do this...
char *sys_get_cmd_line();


unsigned int sys_read_console(FILE_HANDLE file, char *buf, unsigned int size);

int sys_make_dir(const char *path);
FILE_HANDLE sys_get_std_file(int std);
FILE_HANDLE sys_open_file(const char *name, int access, int options);
bool sys_delete_file(const char *name);
void sys_close_file(FILE_HANDLE file);
elf_i64 sys_move_file_cursor(FILE_HANDLE file, int relativeto, elf_i64 dist);
elf_i64 sys_size_file(FILE_HANDLE file);
elf_i64 sys_read_file(FILE_HANDLE file, void *buf, elf_i64 zbuf);
elf_i64 sys_write_file(FILE_HANDLE file, void *buf, elf_i64 zbuf);
// windows will do file buffering by default
void sys_flush_file(FILE_HANDLE file);


int sys_time_file(FILE_HANDLE file, FILE_TIMES *);
void sys_file_time_to_system_time(FILE_TIME *, SYSTEM_TIME *);


void *sys_virtual_alloc(elf_i64 length);
void sys_virtual_free(void *memory);


/* printing, use type (LOG_TYPE),
for instance, for web,
LOG = console.log, ERROR = console.err */
void sys_console_print(int type, char *message);


/* triggers the debugger for this program,
returns whether a debugger was successfully
attached */
bool sys_debugger();


void sys_sleep(elf_i64 ms);


// counts per second
elf_i64 sys_get_performance_counter_frequency();

// get the highest resolution clock available
elf_i64 sys_get_performance_counter();


int sys_get_this_process_id();
void sys_exit_this_process(int errorcode);

int sys_get_work_dir(char *buf, int bufsize);
int sys_set_work_dir(const char *buf);


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


#if defined(PATH_BUILDER)

typedef struct {
	// todo: this could honestly just be in the path builder
	// and we'd have a little api thing for pushing files
	// and dirs, and then the api would allow you to push the
	// info along with the name
	int          type;
	int          size;
	Path_Builder pb;
} FILE_VISITOR;


FILE_HANDLE sys_find_first_file(FILE_VISITOR *visitor);
int sys_find_next_file(FILE_HANDLE hand, FILE_VISITOR *visitor);
void sys_find_close(FILE_HANDLE hand);

#endif


#endif