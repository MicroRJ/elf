/*
** See Copyright Notice In elf.h
** system.h
** Basic system layer
*/


/* virtual alloc */
static void *sys_valloc(elf_Int length);



/* printing, use type (LOG_TYPE),
for instance, for web,
LOG = console.log, ERROR = console.err */
static void sys_console_print(int type, char *message);


/* triggers the debugger for this program,
returns whether a debugger was successfully
attached */
static elf_Bool sys_debugger();


static void sys_sleep(elf_Int ms);


/* the clock frequency, use to translate
clock time to seconds, for web you might get
milliseconds, so freq=1000, for desktop,
you get a performance counter, which has
a nano-second resolution  */
static elf_Int sys_get_clock_freq();

/* get the highest resolution clock available */
static elf_Int sys_get_clock_time();


static int sys_get_my_name(int length, char *text);
static int sys_get_my_pid();

static int sys_get_work_dir(int length, char *text);
static int sys_set_work_dir(char *text);

static elf_Handle sys_load_dll(char const *name);
static void *sys_get_dll_fn(elf_Handle lib, char const *name);

static elf_Error sys_load_file_data(elAllocator alloc, void **pdata, char const *name);
/* use length=0 or length<0 to use strlen */
static elf_Error sys_save_file_data(char const *text, elf_Int length, char const *name);

static int sys_get_last_error();
static void sys_get_error_msg(int error, char *buff, int len);


static int sys_shell(char const *verb, char const *file, char const *args);
static int sys_exec(char const *file, char const *args);


typedef int (*enumerate_folder_callback)(void *user, int fileflags, char *filename, char *filepath);
static int sys_enumerate_folder(elAllocator alloc, char const *file, void *data, enumerate_folder_callback callback);
