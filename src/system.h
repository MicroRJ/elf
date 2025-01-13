/*
** See Copyright Notice In elf.h
** system.h
*/


/* virtual alloc */
static void *sys_valloc(elf_i64 length);


/* printing, use type (LOG_TYPE),
for instance, for web,
LOG = console.log, ERROR = console.err */
static void sys_console_print(int type, char *message);


/* triggers the debugger for this program,
returns whether a debugger was successfully
attached */
static bool sys_debugger();


static void sys_sleep(elf_i64 ms);


/* the clock frequency, use to translate
clock time to seconds, for web you might get
milliseconds, so freq=1000, for desktop,
you get a performance counter, which has
a nano-second resolution  */
static elf_i64 sys_get_clock_freq();

/* get the highest resolution clock available */
static elf_i64 sys_get_clock_time();


static int sys_get_my_name(int length, char *text);
static int sys_get_my_pid();

static int sys_get_work_dir(int length, char *text);
static int sys_set_work_dir(char *text);

static elf_Handle sys_load_dll(char const *name);
static void *sys_get_dll_fn(elf_Handle lib, char const *name);

static int sys_get_last_error();
static void sys_get_error_msg(int error, char *buff, int len);


#if 0
typedef int (*enumerate_folder_callback)(void *user, int filetype, size_t filesize, char *filename, char *filepath);
static int sys_enumerate_folder(Allocator alloc, char const *file, void *data, enumerate_folder_callback callback);
#endif