//
// See Copyright Notice In elf.h
//
#ifndef ELF_OS_SERVICES_H
#define ELF_OS_SERVICES_H

#include "elf_os.h"

#define ELF_OS_PATH_CAPACITY 32768

typedef struct
{
	char *data;
	elf_u64 size;
	elf_u64 capacity;
}
elf_PathBuilder;

typedef elf_u64 elf_PathMark;

typedef struct
{
	uintptr_t value;
}
elf_OS_File;

typedef struct
{
	elf_u64 size;
	elf_i64 created_unix_ms;
	elf_i64 accessed_unix_ms;
	elf_i64 modified_unix_ms;
	elf_b32 is_directory;
	elf_b32 is_symbolic_link;
}
elf_OS_FileInfo;

typedef struct
{
	elf_u64 size;
	elf_u64 required_capacity;
	elf_b32 success;
}
elf_OS_StringResult;

typedef struct
{
	uintptr_t value;
}
elf_OS_Directory;

typedef struct
{
	// The name view remains valid until the next call or the directory is closed.
	const char *name;
	elf_u64 name_size;
	elf_OS_FileInfo info;
}
elf_OS_DirectoryEntry;

typedef enum
{
	ELF_OS_DIRECTORY_ERROR = -1,
	ELF_OS_DIRECTORY_END,
	ELF_OS_DIRECTORY_ENTRY,
}
elf_OS_DirectoryStatus;

elf_u64 elf_os_write_console(elf_OS_StandardStream stream, const void *data, elf_u64 size);
void elf_os_sleep(elf_u64 milliseconds);

elf_b32 elf_path_builder_init(elf_PathBuilder *path, char *storage, elf_u64 capacity, const char *root, elf_u64 root_size);
elf_PathMark elf_path_mark(const elf_PathBuilder *path);
elf_b32 elf_path_push(elf_PathBuilder *path, const char *component, elf_u64 component_size);
void elf_path_pop(elf_PathBuilder *path, elf_PathMark mark);

elf_b32 elf_os_get_file_info(const char *path, elf_OS_FileInfo *info);
elf_OS_File elf_os_open_file_read(const char *path);
elf_OS_File elf_os_create_file(const char *path);
elf_b32 elf_os_file_is_valid(elf_OS_File file);
void elf_os_close_file(elf_OS_File file);
elf_b32 elf_os_read_file(elf_OS_File file, void *data, elf_u64 size, elf_u64 *bytes_read);
elf_b32 elf_os_write_file(elf_OS_File file, const void *data, elf_u64 size, elf_u64 *bytes_written);

elf_b32 elf_os_copy_file(const char *source, const char *destination, elf_b32 overwrite);
elf_b32 elf_os_move_file(const char *source, const char *destination, elf_b32 overwrite);
elf_b32 elf_os_remove_file(const char *path);
elf_b32 elf_os_create_directory(const char *path);
elf_b32 elf_os_create_directories(const char *path);
elf_b32 elf_os_remove_directory(const char *path);
elf_b32 elf_os_remove_tree(const char *path);

elf_OS_StringResult elf_os_get_current_directory(char *buffer, elf_u64 capacity);
elf_b32 elf_os_set_current_directory(const char *path);

elf_OS_DirectoryStatus elf_os_find_first_file(elf_PathBuilder *path, elf_OS_Directory *directory, elf_OS_DirectoryEntry *entry);
elf_OS_DirectoryStatus elf_os_find_next_file(elf_OS_Directory *directory, elf_OS_DirectoryEntry *entry);
void elf_os_close_directory(elf_OS_Directory *directory);

#endif
